#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "analysis.hh"
#include "cmdline.hh"
#include "common.hh"
#include "compile.hh"
#include "confs.hh"
#include "hooks.hh"
#include "link.hh"
#include "packages.hh"
#include "paths.hh"
#include "thread_pool.hh"

// TODO: for each package, find the exported packages in them
// build a graph, check for version clashes, etc,
// then also add them as includes
// static std::vector<std::filesystem::path>
// get_include_directories_for_packages(ConfigurationFile const& config)
// {
//   std::vector<std::filesystem::path> include_dirs;

//   auto const& packages = config.libs.packages;

//   for (auto const& [name, version] : packages) {
//     auto const package = try_get_compatable_package(name, version);

//     if (not package)
//       throw std::runtime_error(
//         std::format("unable to get compatable version <{}> for package <{}> "
//                     "while searching for headers",
//                     version_triplet_to_string(version),
//                     name));

//     auto const package_include_dir = (*package) / "include";

//     include_dirs.push_back(package_include_dir);
//   }

//   return include_dirs;
// }

// helper function to build both
// c/cxx and return the object files
static std::vector<std::filesystem::path>
build_c_cxx(ThreadPool& threads,
            ConfigurationFile const& config,
            TargetFile const& tools,
            std::filesystem::path const& cache,
            PackageCacheDB const& db,
            Deptree const& deptree,
            bool const release,
            bool const pic)
{
  std::vector<std::filesystem::path> include_dirs;

  auto const includes =
    collect_packages_to_include(config, db, tools.triplet, deptree);

  for (auto const& ident : includes)
    include_dirs.push_back(get_packages_include_directory(ident));

  auto [cxx_object_files, cxx_futures] =
    compile_cxx(threads, config, tools, cache, include_dirs, release, pic);

  auto [c_object_files, c_futures] =
    compile_c(threads, config, tools, cache, include_dirs, release, pic);

  auto const obj_files = cxx_object_files + c_object_files;
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

  return obj_files;
}

static void
build_executable(ThreadPool& threads,
                 ConfigurationFile const& config,
                 TargetFile const& target,
                 BuildOptions const& build_opts,
                 PackageCacheDB const& db,
                 Deptree const& deptree,
                 std::filesystem::path const& emit_dir)
{
  // auto const include_dirs = get_include_directories_for_packages(config);
  auto const cache =
    get_cache_folder(target.triplet.to_string(), build_opts.release, false);

  auto object_files = build_c_cxx(
    threads, config, target, cache, db, deptree, build_opts.release, false);

  object_files.push_back(compile_hewgsym(config, target, false));

  link_executable(
    config, target, build_opts, db, deptree, object_files, emit_dir);

  if (build_opts.release)
    run_command("strip", "-s", (emit_dir / config.project.name).string());
}

static void
build_static_library(ThreadPool& threads,
                     ConfigurationFile const& config,
                     TargetFile const& target,
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
                                          non_pic_cache,
                                          db,
                                          deptree,
                                          build_opts.release,
                                          false);
    pack_static_library(config, target, object_files, emit_dir, false);
  }

  {
    auto const pic_cache =
      get_cache_folder(target.triplet.to_string(), build_opts.release, true);
    threadsafe_print("building PIC library code...");
    auto const object_files = build_c_cxx(threads,
                                          config,
                                          target,
                                          pic_cache,
                                          db,
                                          deptree,
                                          build_opts.release,
                                          true);
    pack_static_library(config, target, object_files, emit_dir, true);
  }
}

static void
build_shared_library(ThreadPool& threads,
                     ConfigurationFile const& config,
                     TargetFile const& target,
                     BuildOptions const& build_opts,
                     PackageCacheDB const& db,
                     Deptree const& deptree,
                     std::filesystem::path const& emit_dir)
{
  auto const cache =
    get_cache_folder(target.triplet.to_string(), build_opts.release, true);

  auto object_files = build_c_cxx(
    threads, config, target, cache, db, deptree, build_opts.release, true);
  object_files.push_back(compile_hewgsym(config, target, true));
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
        threads, config, target, build_opts, db, deptree, emit_dir);
      break;

    case PackageType::StaticLibrary:
      build_static_library(
        threads, config, target, build_opts, db, deptree, emit_dir);
      break;

    case PackageType::SharedLibrary: {
      build_shared_library(
        threads, config, target, build_opts, db, deptree, emit_dir);
    } break;

      // header only projects
      // have nothing to compile,
      // just skip
    case PackageType::Headers:
      return;
  }

  triggers_postbuild_hooks(config);
}
