#include "build.hh"
#include "common.hh"
#include "compile.hh"
#include "confs.hh"
#include "hooks.hh"
#include "link.hh"
#include "paths.hh"
#include "thread_pool.hh"

// ensures that a .hcache exists
static void
check_cache()
{
  if (std::filesystem::exists(hewg_cache_path)) {
    if (not std::filesystem::is_directory(hewg_cache_path))
      throw std::runtime_error(
        "hewg cache path, .hcache, should be a directory, but is not!");
  } else {
    threadsafe_print("creating cache directory\n");
    std::filesystem::create_directory(hewg_cache_path);
  }

  // these should also just always exist
  std::filesystem::create_directory(hewg_cxx_object_cache_path);
  std::filesystem::create_directory(hewg_cxx_dependency_cache_path);
  std::filesystem::create_directory(hewg_c_object_cache_path);
  std::filesystem::create_directory(hewg_c_dependency_cache_path);

  std::filesystem::create_directory(hewg_cxx_pic_object_cache_path);
  std::filesystem::create_directory(hewg_cxx_pic_dependency_cache_path);
  std::filesystem::create_directory(hewg_c_pic_object_cache_path);
  std::filesystem::create_directory(hewg_c_pic_dependency_cache_path);
}

static void
build_executable(ThreadPool& threads,
                 ConfigurationFile const& config,
                 BuildOptions const& build_opts,
                 std::filesystem::path const& emit_dir)
{
  auto const object_files =
    compile_c_cxx(threads, config, build_opts.release, false);

  link_executable(config, build_opts, object_files, emit_dir);

  if (build_opts.release)
    run_command("strip", "-s", (emit_dir / config.project.name).string());
}

static void
build_static_library(ThreadPool& threads,
                     ConfigurationFile const& config,
                     BuildOptions const& build_opts,
                     std::filesystem::path const& emit_dir)
{
  auto const object_files =
    compile_c_cxx(threads, config, build_opts.release, false);
  auto const object_files_pic =
    compile_c_cxx(threads, config, build_opts.release, true);

  pack_static_library(config, object_files, emit_dir, false);
  pack_static_library(config, object_files, emit_dir, true);
}

void
build(ThreadPool& threads,
      ConfigurationFile const& config,
      BuildOptions const& build_opts,
      std::string_view build_profile)
{
  // basic checks
  check_cache();

  trigger_prebuild_hooks(config);

  std::vector<std::filesystem::path> object_files;

  create_directory_checked(hewg_target_directory_path);
  create_directory_checked(hewg_target_directory_path / build_profile);

  auto const emit_dir = hewg_target_directory_path / build_profile;

  switch (config.meta.type) {
    case ProjectType::Executable:
      build_executable(threads, config, build_opts, emit_dir);
      break;

    case ProjectType::StaticLibrary:
      build_static_library(threads, config, build_opts, emit_dir);
      break;

    case ProjectType::SharedLibrary: {
      threadsafe_print("shared library building not yet supported");
    } break;

      // header only projects
      // have nothing to compile,
      // just skip
    case ProjectType::Headers:
      return;
  }

  triggers_postbuild_hooks(config);
}
