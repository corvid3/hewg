#include <algorithm>
#include <filesystem>
#include <fstream>
#include <print>
#include <stdexcept>
#include <vector>

#include "analysis.hh"
#include "cmdline.hh"
#include "common.hh"
#include "compile.hh"
#include "confs.hh"
#include "crow.jayson/jayson.hh"
#include "deptree.hh"
#include "hooks.hh"
#include "link.hh"
#include "packages.hh"
#include "paths.hh"
#include "srcrelatives.hh"
#include "thread_pool.hh"

// helper function to build both
// c/cxx and return the object files
static std::vector<std::filesystem::path>
build_c_cxx(ThreadPool& threads,
            ConfigurationFile const& config,
            TargetFile const& tools,
            PackageIdentifier const& this_package_ident,
            std::filesystem::path const& cache,
            std::filesystem::path const& c_src_folder,
            std::filesystem::path const& cxx_src_folder,
            PackageCacheDB const& db,
            Deptree const& deptree,
            bool const release,
            bool const pic,
            bool const gen_cc)
{
  std::vector<std::filesystem::path> include_dirs;

  auto const includes =
    collect_packages_to_include(config, db, tools.triplet, deptree);

  for (auto const& ident : includes)
    include_dirs.push_back(get_packages_include_directory(ident));

  std::vector<std::filesystem::path> cxx_paths;
  std::ranges::transform(config.cxx.sources,
                         std::inserter(cxx_paths, cxx_paths.end()),
                         [](std::string_view const in) { return in; });

  std::vector<std::filesystem::path> c_paths;
  std::ranges::transform(config.c.sources,
                         std::inserter(c_paths, c_paths.end()),
                         [](std::string_view const in) { return in; });

  CSourceRelatives csrc(cache, c_src_folder, c_paths);
  CXXSourceRelatives cxxsrc(cache, cxx_src_folder, cxx_paths);

  auto const c_flags = generate_c_flags(config.c.flags,
                                        include_dirs,
                                        config.c.std.value_or(23),
                                        this_package_ident,
                                        release,
                                        pic);

  auto const cxx_flags = generate_cxx_flags(config.cxx.flags,
                                            include_dirs,
                                            config.cxx.std.value_or(23),
                                            this_package_ident,
                                            release,
                                            pic);

  if (gen_cc) {
    jayson::array database;
    jayson::array cxx_args{ tools.cxx };
    jayson::array c_args{ tools.cc };

    for (auto const& flag : c_flags)
      c_args.push_back(flag);
    for (auto const& flag : cxx_flags)
      cxx_args.push_back(flag);

    for (auto const& file : csrc.files()) {
      jayson::obj fragment;
      fragment["directory"] = std::filesystem::current_path();
      fragment["arguments"] = c_args;
      fragment["file"] = "csrc" / file.source;
      database.push_back(std::move(fragment));
    }

    for (auto const& file : cxxsrc.files()) {
      jayson::obj fragment;
      fragment["directory"] = std::filesystem::current_path();
      fragment["arguments"] = cxx_args;
      fragment["file"] = "src" / file.source;
      database.push_back(std::move(fragment));
    }

    std::ofstream("compile_commands.json")
      << jayson::val(database).serialize(true);
  }

  auto cxx_futures = compile_cxx(threads, cxx_flags, cxxsrc, tools);
  auto c_futures = compile_c(threads, c_flags, csrc, tools);
  auto futures = std::move(cxx_futures) + std::move(c_futures);

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

static void
build_executable(ThreadPool& threads,
                 ConfigurationFile const& config,
                 TargetFile const& target,
                 PackageIdentifier const& this_package_ident,
                 BuildOptions const& build_opts,
                 PackageCacheDB const& db,
                 Deptree const& deptree,
                 std::filesystem::path const& emit_dir)
{
  // auto const include_dirs = get_include_directories_for_packages(config);
  auto const cache =
    get_cache_folder(target.triplet.to_string(), build_opts.release, false);

  auto object_files = build_c_cxx(threads,
                                  config,
                                  target,
                                  this_package_ident,
                                  cache,
                                  hewg_c_src_directory_path,
                                  hewg_cxx_src_directory_path,
                                  db,
                                  deptree,
                                  build_opts.release,
                                  false,
                                  build_opts.gen_compile_commands);

  object_files.push_back(
    compile_hewgsym(config, target, this_package_ident, false));

  link_executable(
    config, target, build_opts, db, deptree, object_files, emit_dir);

  if (build_opts.release)
    run_command("strip", "-s", (emit_dir / config.project.name).string());
}

static void
build_static_library(ThreadPool& threads,
                     ConfigurationFile const& config,
                     TargetFile const& target,
                     PackageIdentifier const& this_package_ident,
                     BuildOptions const& build_opts,
                     PackageCacheDB const& db,
                     Deptree const& deptree,
                     std::filesystem::path const& emit_dir)
{

  {
    auto const non_pic_cache =
      get_cache_folder(target.triplet.to_string(), build_opts.release, false);
    threadsafe_print("building non-PIC library code...");
    auto const object_files = build_c_cxx(threads,
                                          config,
                                          target,
                                          this_package_ident,
                                          non_pic_cache,
                                          hewg_c_src_directory_path,
                                          hewg_cxx_src_directory_path,
                                          db,
                                          deptree,
                                          build_opts.release,
                                          false,
                                          build_opts.gen_compile_commands);
    pack_static_library(config, target, object_files, emit_dir, false);
  }

  {
    auto const pic_cache =
      get_cache_folder(target.triplet.to_string(), build_opts.release, true);
    threadsafe_print("building PIC library code...");
    auto const object_files = build_c_cxx(threads,
                                          config,
                                          target,
                                          this_package_ident,
                                          pic_cache,
                                          hewg_c_src_directory_path,
                                          hewg_cxx_src_directory_path,
                                          db,
                                          deptree,
                                          build_opts.release,
                                          true,
                                          build_opts.gen_compile_commands);
    pack_static_library(config, target, object_files, emit_dir, true);
  }
}

static void
build_shared_library(ThreadPool& threads,
                     ConfigurationFile const& config,
                     TargetFile const& target,
                     PackageIdentifier const& this_package_ident,
                     BuildOptions const& build_opts,
                     PackageCacheDB const& db,
                     Deptree const& deptree,
                     std::filesystem::path const& emit_dir)
{
  auto const cache =
    get_cache_folder(target.triplet.to_string(), build_opts.release, true);

  auto object_files = build_c_cxx(threads,
                                  config,
                                  target,
                                  this_package_ident,
                                  cache,
                                  hewg_c_src_directory_path,
                                  hewg_cxx_src_directory_path,
                                  db,
                                  deptree,
                                  build_opts.release,
                                  true,
                                  build_opts.gen_compile_commands);

  object_files.push_back(
    compile_hewgsym(config, target, this_package_ident, true));

  shared_link(config, target, build_opts, db, deptree, object_files, emit_dir);
}

void
build(ThreadPool& threads,
      ConfigurationFile const& config,
      PackageCacheDB& db,
      TargetFile const& target,
      BuildOptions const& build_opts)
{
  // get the dependency graph
  trigger_prebuild_hooks(config);

  std::vector<std::filesystem::path> object_files;

  create_directory_checked(hewg_target_directory_path);
  create_directory_checked(hewg_target_directory_path /
                           target.triplet.to_string());

  auto const ident = get_this_package_ident(config, target.triplet);
  auto const emit_dir = get_artifact_folder(ident);
  auto const deptree = build_dependency_tree(config, db, target.triplet);

  switch (config.meta.type) {
    case PackageType::Executable:
      build_executable(
        threads, config, target, ident, build_opts, db, deptree, emit_dir);
      break;

    case PackageType::StaticLibrary:
      build_static_library(
        threads, config, target, ident, build_opts, db, deptree, emit_dir);
      break;

    case PackageType::SharedLibrary: {
      build_shared_library(
        threads, config, target, ident, build_opts, db, deptree, emit_dir);
    } break;

      // header only projects
      // have nothing to compile,
      // just skip
    case PackageType::Headers:
      return;
  }

  triggers_postbuild_hooks(config);
}
