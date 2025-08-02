#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <jayson.hh>

#include "analysis.hh"
#include "build.hh"
#include "cmdline.hh"
#include "common.hh"
#include "compile.hh"
#include "compile_commands.hh"
#include "confs.hh"
#include "hooks.hh"
#include "link.hh"
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

static std::vector<std::string>
generate_compile_arguments(std::string const& compiler,
                           std::string const& standard,
                           std::vector<std::string> const& flags,
                           std::string const& relative_source_path)
{
  return std::vector<std::string>{
    compiler,
    "-c",
    "-Iprivate",
    "-Iinclude",
    standard,
    relative_source_path
  } + flags;
}

static void
generate_compile_commands(ConfigurationFile const& config,
                          ToolFile const& tools)
{
  std::vector<CompileCommand> commands;

  auto const cxx_filepaths = get_cxx_source_filepaths(config);
  for (auto const& filepath : cxx_filepaths) {
    std::string const relative_source_path = std::filesystem::relative(filepath);

    commands.push_back(CompileCommand{
      std::filesystem::current_path(),
      generate_compile_arguments(tools.cxx,
                                std::format("-std={}", get_cxx_standard_string(config.cxx.std.value_or(20))),
                                config.cxx.flags,
                                relative_source_path),
      relative_source_path
    });
  }

  auto const c_filepaths = get_c_source_filepaths(config);
  for (auto const& filepath : c_filepaths) {
    std::string const relative_source_path = std::filesystem::relative(filepath);

    commands.push_back(CompileCommand{
      std::filesystem::current_path(),
      generate_compile_arguments(tools.cc,
                                 std::format("-std={}", get_c_standard_string(config.c.std.value_or(17))),
                                 config.cxx.flags,
                                 relative_source_path),
      relative_source_path
    });
  }


  std::ofstream("compile_commands.json") << serialize_compile_commands(commands);
}

// helper function to build both
// c/cxx and return the object files
static std::vector<std::filesystem::path>
build_c_cxx(ThreadPool& threads,
            ConfigurationFile const& config,
            ToolFile const& tools,
            std::filesystem::path const& cache,
            bool release,
            bool pic)
{
  auto [cxx_object_files, cxx_futures] =
    compile_cxx(threads, config, tools, cache, release, pic);

  auto [c_object_files, c_futures] =
    compile_c(threads, config, tools, cache, release, pic);

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
                 ToolFile const& tools,
                 BuildOptions const& build_opts,
                 std::string_view build_profile,
                 std::filesystem::path const& emit_dir)
{
  // auto const include_dirs = get_include_directories_for_packages(config);
  auto const cache = get_cache_folder(build_profile, build_opts.release, false);
  auto object_files =
    build_c_cxx(threads, config, tools, cache, build_opts.release, false);

  object_files.push_back(compile_hewgsym(config, tools, false));

  link_executable(config, tools, build_opts, object_files, emit_dir);

  if (build_opts.release)
    run_command("strip", "-s", (emit_dir / config.project.name).string());
}

[[maybe_unused]] static void
build_static_library(ThreadPool& threads [[maybe_unused]],
                     ConfigurationFile const& config [[maybe_unused]],
                     BuildOptions const& build_opts [[maybe_unused]],
                     std::filesystem::path const& emit_dir [[maybe_unused]])
{
  // auto const include_dirs = get_include_directories_for_packages(config);

  // threadsafe_print("building non-PIC library code...");
  // auto const object_files =
  //   compile_c_cxx(threads, config, {}, build_opts.release, false);

  // threadsafe_print("building PIC library code...");
  // auto const object_files_pic =
  //   compile_c_cxx(threads, config, {}, build_opts.release, true);

  // pack_static_library(config, object_files, emit_dir, false);
  // pack_static_library(config, object_files, emit_dir, true);
}

static void
build_shared_library(ThreadPool& threads,
                     ConfigurationFile const& config,
                     ToolFile const& tools,
                     BuildOptions const& build_opts,
                     std::string_view build_profile,
                     std::filesystem::path const& emit_dir)
{
  auto const cache = get_cache_folder(build_profile, build_opts.release, true);

  auto object_files =
    build_c_cxx(threads, config, tools, cache, build_opts.release, true);
  object_files.push_back(compile_hewgsym(config, tools, true));
  shared_link(config, tools, build_opts, object_files, emit_dir);
}

void
build(ThreadPool& threads,
      ConfigurationFile const& config,
      BuildOptions const& build_opts,
      std::string_view build_profile)
{
  ToolFile const tools = get_tool_file(config, build_profile);

  if (build_opts.generate_compile_commands) {
    // just create the compile_commands.json and exit
    threadsafe_print(std::format("writing: {}/compile_commands.json", std::filesystem::current_path().string()));
    do_terminal_countdown(3);
    generate_compile_commands(config, tools);
    return;
  }
  // get the dependency graph
  trigger_prebuild_hooks(config);

  std::vector<std::filesystem::path> object_files;

  create_directory_checked(hewg_target_directory_path);
  create_directory_checked(hewg_target_directory_path / build_profile);

  auto const emit_dir = hewg_target_directory_path / build_profile;

  switch (config.meta.type) {
    case ProjectType::Executable:
      build_executable(
        threads, config, tools, build_opts, build_profile, emit_dir);
      break;

    case ProjectType::StaticLibrary:
      threadsafe_print("static library building not yet supported");
      // build_static_library(threads, config, tools, build_opts, emit_dir);
      break;

    case ProjectType::SharedLibrary: {
      threadsafe_print("shared library building not yet supported");
      build_shared_library(
        threads, config, tools, build_opts, build_profile, emit_dir);
    } break;

      // header only projects
      // have nothing to compile,
      // just skip
    case ProjectType::Headers:
      return;
  }

  triggers_postbuild_hooks(config);
}