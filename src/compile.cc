#include "analysis.hh"
#include "cmdline.hh"
#include "common.hh"
#include "compile.hh"
#include "confs.hh"
#include "paths.hh"
#include <algorithm>
#include <atomic>
#include <exception>
#include <filesystem>
#include <iterator>

auto
get_common_c_cxx_flags(std::filesystem::path const file,
                       std::filesystem::path const dep_filepath,
                       std::filesystem::path const obj_filepath,
                       bool const is_release)
{
  std::vector<std::string> args;
  args.push_back("-c");
  args.push_back("-MMD");
  args.push_back("-MF");
  args.push_back(std::filesystem::relative(dep_filepath));
  args.push_back("-o");
  args.push_back(std::filesystem::relative(obj_filepath));
  args.push_back(std::filesystem::relative(file));

  if (is_release) {
    args.push_back("-O2");
  } else {
    args.push_back("-Og");
    args.push_back("-g");
  }

  return args;
}

// will set successful_compile to false
// if the task failed
static void
start_cxx_compile_task(ThreadPool& thread_pool,
                       ConfigurationFile const& config,
                       BuildOptions const& options,
                       std::filesystem::path const file,
                       std::atomic<bool>& successful_compile)
{
  thread_pool.add_job([=, &config, &thread_pool, &successful_compile]() {
    try {
      auto const obj_filepath = object_file_for(file);
      auto const dep_filepath = depfile_for(file);

      // create the directories for the cache
      std::filesystem::create_directories(obj_filepath.parent_path());

      auto args = get_common_c_cxx_flags(
        file, dep_filepath, obj_filepath, options.release);

      args.insert(args.end(),
                  config.flags.cxx_flags.begin(),
                  config.flags.cxx_flags.end());

      auto const code = run_command("c++", args);

      if (code != 0) {
        // mark the complie as a failure
        // and get rid of all future compile tasks
        successful_compile = false;
        thread_pool.drain();
      }
    } catch (std::exception const& e) {
      successful_compile = false;
      throw;
    }
  });
}

static void
start_c_compile_task(ThreadPool& thread_pool,
                     ConfigurationFile const& config,
                     BuildOptions const& options,
                     std::filesystem::path const file,
                     std::atomic<bool>& successful_compile)
{
  thread_pool.add_job([=, &config, &thread_pool, &successful_compile]() {
    auto const obj_filepath = object_file_for(file);
    auto const dep_filepath = depfile_for(file);

    // create the directories for the cache
    std::filesystem::create_directories(obj_filepath.parent_path());

    auto args =
      get_common_c_cxx_flags(file, dep_filepath, obj_filepath, options.release);

    args.insert(
      args.end(), config.flags.c_flags.begin(), config.flags.c_flags.end());

    auto const code = run_command("c", args);

    if (code != 0) {
      // mark the complie as a failure
      // and get rid of all future compile tasks
      successful_compile = false;
      thread_pool.drain();
    }
  });
}

void
link(ConfigurationFile const& config,
     std::span<std::filesystem::path const> object_files,
     std::filesystem::path output_directory)
{
  if (not std::filesystem::is_directory(output_directory))
    throw std::runtime_error("output_directory in link() isn't a directory");

  std::vector<std::string> args;

  args.push_back("-o");
  args.push_back(output_directory / config.project.name);

  for (auto const& of : object_files)
    args.push_back(std::filesystem::relative(of).string());

  for (auto const& lib : config.libs.privates)
    args.push_back(std::format("-l{}", lib));

  run_command("c++", args);
}

std::vector<std::filesystem::path>
compile_c_cxx(ThreadPool& pool,
              ConfigurationFile const& config,
              BuildOptions const& options)
{
  auto const source_filepaths = get_source_filepaths(config);
  auto const rebuilds = mark_c_cxx_files_for_rebuild(source_filepaths);

  auto const cxx_rebuilds = get_files_by_type(rebuilds, FileType::CXXSource);
  auto const c_rebuilds = get_files_by_type(rebuilds, FileType::CSource);

  std::atomic<bool> successful_compile(true);

  for (auto const& rebuild : c_rebuilds)
    start_c_compile_task(pool, config, options, rebuild, successful_compile);

  for (auto const& rebuild : cxx_rebuilds)
    start_cxx_compile_task(pool, config, options, rebuild, successful_compile);

  pool.block_until_finished();

  if (not successful_compile)
    throw std::runtime_error("fatal errors when compiling cxx source files");

  std::vector<std::filesystem::path> object_files;
  auto c_objs = get_files_by_type(source_filepaths, FileType::CSource);
  auto cxx_objs = get_files_by_type(source_filepaths, FileType::CXXSource);

  std::ranges::transform(c_objs, c_objs.begin(), object_file_for);
  std::ranges::transform(cxx_objs, cxx_objs.begin(), object_file_for);

  std::ranges::set_union(
    c_objs, cxx_objs, std::inserter(object_files, object_files.end()));

  return object_files;
}
