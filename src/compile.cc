#include "analysis.hh"
#include "common.hh"
#include "compile.hh"
#include "confs.hh"
#include "paths.hh"
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iterator>

// will set successful_compile to false
// if the task failed
static void
start_cxx_compile_task(ThreadPool& thread_pool,
                       ConfigurationFile const& config,
                       std::filesystem::path const file,
                       std::atomic<bool>& successful_compile)
{
  thread_pool.add_job([=, &config, &thread_pool, &successful_compile]() {
    auto const obj_filepath = object_file_for(file);
    auto const dep_filepath = depfile_for(file);

    // create the directories for the cache
    std::filesystem::create_directories(obj_filepath.parent_path());

    std::vector<std::string> args;
    args.push_back("-c");
    args.push_back("-MMD");
    args.push_back("-MF");
    args.push_back(std::filesystem::relative(dep_filepath));
    args.push_back("-o");
    args.push_back(std::filesystem::relative(obj_filepath));
    args.push_back(std::filesystem::relative(file));
    args.insert(
      args.end(), config.flags.cxx_flags.begin(), config.flags.cxx_flags.end());

    auto const code = run_command("c++", args);

    if (code != 0) {
      // mark the complie as a failure
      // and get rid of all future compile tasks
      successful_compile = false;
      thread_pool.drain();
    }
  });
}

static void
link_c_cxx(ConfigurationFile const& config,
           std::span<std::filesystem::path const> source_files)
{
  std::vector<std::string> args;

  args.push_back("-o");
  args.push_back(config.project.name);

  for (auto const& cxx_file : source_files)
    args.push_back(
      std::filesystem::relative(object_file_for(cxx_file)).string());

  run_command("c++", args);
}

void
compile_c_cxx(ThreadPool& pool, ConfigurationFile const& config)
{
  auto const source_filepaths = get_source_filepaths(config);
  auto const cxx_rebuild = get_cxx_files_to_rebuild(source_filepaths);

  std::atomic<bool> successful_compile(true);

  for (auto const& rebuild : cxx_rebuild)
    start_cxx_compile_task(pool, config, rebuild, successful_compile);

  // TODO: compile C files here

  pool.block_until_finished();

  if (not successful_compile)
    throw std::runtime_error("fatal errors when compiling cxx source files");

  // link everything if all object files successfully compiled
  link_c_cxx(config, get_files_by_type(source_filepaths, FileType::CXXSource));
}
