#include "analysis.hh"
#include "app.hh"
#include "build.hh"
#include "cmdline.hh"
#include "common.hh"
#include "compile.hh"
#include "confs.hh"
#include "crow.jayson/jayson.hh"
#include "deptree.hh"
#include "hooks.hh"
#include "install.hh"
#include "link.hh"
#include "packages.hh"
#include "paths.hh"
#include "srcrelatives.hh"
#include "target.hh"
#include "thread_pool.hh"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <future>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
auto
build_c_cxx(AppContext const& ctx, BuildContext const& build_ctx)
  -> std::vector<std::filesystem::path>
{
  std::vector<std::filesystem::path> cxx_paths;
  std::ranges::transform(ctx.config().cxx.sources,
                         std::inserter(cxx_paths, cxx_paths.end()),
                         [](std::string_view const in) {
                           return in;
                         });

  std::vector<std::filesystem::path> c_paths;
  std::ranges::transform(ctx.config().c.sources,
                         std::inserter(c_paths, c_paths.end()),
                         [](std::string_view const in) {
                           return in;
                         });

  CSourceRelatives csrc(build_ctx.cache(), hewg_c_src_directory_path, c_paths);
  CXXSourceRelatives cxxsrc(
    build_ctx.cache(), hewg_cxx_src_directory_path, cxx_paths);

  if (ctx.build_options().gen_compile_commands) {
    jayson::array database;
    jayson::array cxx_args{ build_ctx.target().cxx };
    jayson::array c_args{ build_ctx.target().cc };

    for (auto const& flag : build_ctx.cflags())
      c_args.emplace_back(flag);
    for (auto const& flag : build_ctx.cxxflags())
      cxx_args.emplace_back(flag);

    for (auto const& file : csrc.files()) {
      jayson::obj fragment;
      fragment["directory"] = std::filesystem::current_path();
      fragment["arguments"] = c_args;
      fragment["file"]      = "csrc" / file.source;
      database.emplace_back(std::move(fragment));
    }

    for (auto const& file : cxxsrc.files()) {
      jayson::obj fragment;
      fragment["directory"] = std::filesystem::current_path();
      fragment["arguments"] = cxx_args;
      fragment["file"]      = "src" / file.source;
      database.emplace_back(std::move(fragment));
    }

    std::ofstream("compile_commands.json")
      << jayson::val(database).serialize(true);
  }

  auto cxx_futures = compile_cxx(ctx, build_ctx, cxxsrc);
  auto c_futures   = compile_c(ctx, build_ctx, csrc);
  auto futures     = std::move(cxx_futures) + std::move(c_futures);

  std::vector<std::string> failed_compiles;

  for (auto& future : futures) {
    auto const val = future.get();

    if (val)
      failed_compiles.push_back(*val);
  }

  if (not failed_compiles.empty()) {
    threadsafe_print("errors in files:\n");
    std::ranges::for_each(failed_compiles, [](auto const& file) {
      threadsafe_print("\t", file, '\n');
    });

    throw std::runtime_error("fatal errors when compiling cxx source files");
  }

  std::vector<std::filesystem::path> obj_files;
  for (auto const& f : cxxsrc.files())
    obj_files.push_back(f.object);
  for (auto const& f : csrc.files())
    obj_files.push_back(f.object);

  return obj_files;
}
}

void
build_executable(AppContext const& ctx, PackageContext const& pkg)
{
  BuildContext const build_ctx(ctx, pkg, false);
  auto               object_files = build_c_cxx(ctx, build_ctx);
  object_files.push_back(compile_hewgsym(ctx, build_ctx));
  link_executable(ctx, pkg, build_ctx, object_files);
  if (ctx.build_options().release) {
    run_command("strip",
                "-s",
                (build_ctx.emitdir() / ctx.config().project.name).string());
  }
}

void
build_static_library(AppContext const& ctx, PackageContext const& pkg)
{

  {
    BuildContext const build_ctx(ctx, pkg, false);
    threadsafe_print("building non-PIC library code...");
    auto const object_files = build_c_cxx(ctx, build_ctx);
    pack_static_library(ctx, build_ctx, object_files);
  }

  {
    BuildContext const build_ctx(ctx, pkg, true);
    threadsafe_print("building PIC library code...");
    auto const object_files = build_c_cxx(ctx, build_ctx);
    pack_static_library(ctx, build_ctx, object_files);
  }
}

void
build_shared_library(AppContext const& ctx, PackageContext const& pkg)
{
  BuildContext const build_ctx(ctx, pkg, true);
  auto               object_files = build_c_cxx(ctx, build_ctx);
  object_files.push_back(compile_hewgsym(ctx, build_ctx));
  shared_link(ctx, pkg, build_ctx, object_files);
}

extern void
build(AppContext const& ctx)
{
  trigger_prebuild_hooks(ctx.config());

  std::vector<std::filesystem::path> object_files;

  create_directory_checked(hewg_target_directory_path);
  create_directory_checked(hewg_target_directory_path
                           / ctx.triplet().to_string());

  PackageContext const pkg(ctx);

  switch (ctx.config().meta.type) {
  case PackageType::Executable   : build_executable(ctx, pkg); break;
  case PackageType::StaticLibrary: build_static_library(ctx, pkg); break;
  case PackageType::SharedLibrary:
    build_shared_library(ctx, pkg);
    break;

    // header only projects
    // have nothing to compile,
    // just skip
  case PackageType::Headers: return;
  }

  triggers_postbuild_hooks(ctx.config());

  if (ctx.build_options().install)
    install(ctx, pkg);
}

BuildContext::BuildContext(AppContext const&     ctx,
                           PackageContext const& pkg,
                           bool const            PIC)
  : m_target(TargetFile::Load(ctx.triplet()))
  , m_thisIdent(get_this_package_ident(ctx.config(), ctx.triplet()))
  , m_cacheFolder(get_cache_folder(ctx.triplet().to_string(),
                                   ctx.build_options().release,
                                   false))
  , m_pic(PIC)
  , m_outputDir(get_artifact_folder(m_thisIdent))
  , m_cFlags(generate_c_flags(ctx, ident(), PIC, pkg.includes()))
  , m_cxxFlags(generate_cxx_flags(ctx, ident(), PIC, pkg.includes())) {};

PackageContext::PackageContext(AppContext const& ctx)
  : m_db(open_package_db())
  , m_tree(build_dependency_tree(ctx.config(), m_db, ctx.triplet()))
  , m_includedPackages(collect_packages_to_include(ctx, *this))
{
}
