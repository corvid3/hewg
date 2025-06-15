#include "analysis.hh"
#include "cmdline.hh"
#include "common.hh"
#include "compile.hh"
#include "confs.hh"
#include "paths.hh"
#include "thread_pool.hh"
#include <algorithm>
#include <atomic>
#include <exception>
#include <filesystem>
#include <iterator>
#include <jayson.hh>

// TODO: for every task i spawn, i also run get_common_flags,
// i should only need to call it once and then just pass a ref to it

static auto
get_common_c_cxx_flags(ConfigurationFile const& config,
                       std::filesystem::path const file,
                       std::filesystem::path const dep_filepath,
                       std::filesystem::path const obj_filepath,
                       bool const is_release)
{
  std::vector<std::string> args;

  if (config.meta.type == ProjectType::SharedLibrary) {
    args.push_back("-fPIC");
  }

  args.push_back("-c");
  args.push_back("-Iprivate");
  args.push_back("-Iinclude");
  args.push_back("-MMD");
  args.push_back("-MF");
  args.push_back(std::filesystem::relative(dep_filepath));
  args.push_back("-o");
  args.push_back(std::filesystem::relative(obj_filepath));
  args.push_back(std::filesystem::relative(file));

  // force enable coloured output
  args.push_back("-fdiagnostics-color=always");

  if (is_release) {
    args.push_back("-O2");
  } else {
    args.push_back("-Og");
    args.push_back("-g");
  }

  return args;
}

static auto
get_library_flags(ConfigurationFile const& config,
                  bool const ignore_static_libraries)
{
  (void)ignore_static_libraries;

  std::vector<std::string> args;

  for (auto const& native_library : config.libs.native)
    args.push_back(std::format("-l{}", native_library));

  for (auto const& packaged_library : config.libs.packages) {
    (void)packaged_library;
  }

  return args;
}

static auto
generate_link_flags(ConfigurationFile const& config,
                    bool const is_release,
                    std::span<std::filesystem::path const> object_files,
                    std::filesystem::path const output_filepath)
{
  std::vector<std::string> args;

  args.insert(args.end(), object_files.begin(), object_files.end());
  std::ranges::transform(args, args.begin(), [](auto const& file) {
    return std::filesystem::relative(file);
  });

  args.push_back("-o");
  args.push_back(output_filepath);

  if (config.tools.ld_tool)
    args.push_back(std::format("-fuse-ld={}", *config.tools.ld_tool));

  if (is_release)
    args.push_back("-flto");

  // TODO: add options for libraries

  return args;
}

static void
write_error_file(std::filesystem::path src_file, std::string_view what)
{
  auto const relative =
    std::filesystem::relative(src_file, hewg_src_directory_path);
  auto const file = hewg_err_directory_path / relative;

  std::filesystem::create_directories(file.parent_path());
  std::ofstream(file) << what;

  // strip the output file of console codes with sed,
  // if there's any
  std::vector<std::string> sed_args;
  sed_args.push_back(R"(s/\x1B\[[0-9;]*[mKG]//g)");
  sed_args.push_back(file);
  sed_args.push_back("-i");

  run_command("sed", sed_args);
}

// will set successful_compile to false
// if the task failed
static void
start_cxx_compile_task(ThreadPool& thread_pool,
                       ConfigurationFile const& config,
                       BuildOptions const& options,
                       std::filesystem::path const file,
                       atomic_vec<std::string>& failed_compiles)
{
  thread_pool.add_job([=, &config, &thread_pool, &failed_compiles]() {
    auto const obj_filepath = object_file_for(file);
    auto const dep_filepath = depfile_for(file);

    // create the directories for the cache
    std::filesystem::create_directories(obj_filepath.parent_path());

    auto args = get_common_c_cxx_flags(
      config, file, dep_filepath, obj_filepath, options.release);

    append_vec(args, config.flags.cxx_flags);

    threadsafe_print(std::format(
      "compiling CXX file: <{}>\n",
      std::filesystem::relative(file, hewg_src_directory_path).string()));

    auto const [code, what] =
      run_command(config.tools.cxx_tool.value_or("c++"), args);

    if (code != 0) {
      // mark the complie as a failure
      // and get rid of all future compile tasks
      failed_compiles.push_back(
        (std::filesystem::relative(file, hewg_src_directory_path)).string());
      thread_pool.drain();

      write_error_file(file, what);

      std::ofstream();
    }
  });
}

static void
start_c_compile_task(ThreadPool& thread_pool,
                     ConfigurationFile const& config,
                     BuildOptions const& options,
                     std::filesystem::path const file,
                     atomic_vec<std::string>& failed_compiles)
{
  thread_pool.add_job([=, &config, &thread_pool, &failed_compiles]() {
    auto const obj_filepath = object_file_for(file);
    auto const dep_filepath = depfile_for(file);

    // create the directories for the cache
    std::filesystem::create_directories(obj_filepath.parent_path());

    auto args = get_common_c_cxx_flags(
      config, file, dep_filepath, obj_filepath, options.release);

    args.insert(
      args.end(), config.flags.c_flags.begin(), config.flags.c_flags.end());

    threadsafe_print(std::format(
      "compiling C file: <{}>\n",
      std::filesystem::relative(file, hewg_src_directory_path).string()));

    auto const [code, what] =
      run_command(config.tools.c_tool.value_or("cc"), args);

    if (code != 0) {
      // mark the complie as a failure
      // and get rid of all future compile tasks
      failed_compiles.push_back(
        (std::filesystem::relative(file, hewg_src_directory_path)).string());
      thread_pool.drain();

      write_error_file(file, what);
    }
  });
}

void
link(ConfigurationFile const& config,
     BuildOptions const& options,
     std::span<std::filesystem::path const> object_files,
     std::filesystem::path output_directory)
{
  if (not std::filesystem::is_directory(output_directory))
    throw std::runtime_error("output_directory in link() isn't a directory");

  auto const output_filepath = output_directory / config.project.name;

  auto args =
    generate_link_flags(config, options.release, object_files, output_filepath);
  threadsafe_print("now lets get linking...\n");
  append_vec(args, get_library_flags(config, false));

  run_command("c++", args);
}

void
pack_static_library(ConfigurationFile const& config,
                    std::span<std::filesystem::path const> object_files,
                    std::filesystem::path output_directory)
{
  if (not std::filesystem::is_directory(output_directory))
    throw std::runtime_error(
      "output_directory in pack_static_library() isn't a directory");

  std::filesystem::path const outfile =
    output_directory / std::format("lib{}.a", config.project.name);

  std::vector<std::string> commands;
  commands.push_back("rcs");
  commands.push_back(outfile.string());
  for (auto const& objects : object_files)
    commands.push_back(objects.string());
  run_command("ar", commands);
}

void
shared_link(ConfigurationFile const& config,
            BuildOptions const& options,
            std::span<std::filesystem::path const> object_files,
            std::filesystem::path output_directory)
{
  if (not std::filesystem::is_directory(output_directory))
    throw std::runtime_error(
      "output_directory in shared_link() isn't a directory");

  std::filesystem::path const outfile =
    output_directory / std::format("lib{}.so", config.project.name);

  std::vector<std::string> args =
    generate_link_flags(config, options.release, object_files, outfile);

  // ignore static libraries here
  // and defer their linking by adding them to the
  // descriptor of the package file
  append_vec(args, get_library_flags(config, true));

  args.push_back("-shared");

  run_command("c++", args);
}

static std::string
emit_symcache_contents(std::string_view package_name,
                       version_triplet const trip)
{
  auto const [x, y, z] = trip;

  return std::format("int __hewg_version_package_{}[3] = {{ {}, {}, {} }};",
                     package_name,
                     x,
                     y,
                     z);
}

// returns true if requesting a rebuild of the builtin symbol file
static bool
check_symcache(ConfigurationFile const& config)
{
  if (not std::filesystem::exists(hewg_builtinsym_cache_path)) {
    std::ofstream(hewg_builtinsym_cache_path)
      << jayson::serialize(config.project.version).serialize();

    std::ofstream(hewg_builtinsym_src_path)
      << emit_symcache_contents(config.project.name, config.project.version);

    return true;
  } else {
    std::stringstream ss;
    ss << std::ifstream(hewg_builtinsym_cache_path).rdbuf();

    jayson::val v = jayson::val::parse(std::move(ss).str());
    version_triplet trip;
    jayson::deserialize(v, trip);

    // if the version has changed, request a rebuild
    if (trip != config.project.version) {
      std::ofstream(hewg_builtinsym_src_path)
        << emit_symcache_contents(config.project.name, config.project.version);

      std::ofstream(hewg_builtinsym_cache_path)
        << jayson::serialize(config.project.version).serialize();

      return true;
    } else
      return false;
  }
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

  if (check_symcache(config)) {
    run_command("cc",
                hewg_builtinsym_src_path.string(),
                "-O2",
                "-c",
                "-o",
                hewg_builtinsym_obj_path.string());
  }

  atomic_vec<std::string> failed_compiles;

  for (auto const& rebuild : c_rebuilds)
    start_c_compile_task(pool, config, options, rebuild, failed_compiles);

  for (auto const& rebuild : cxx_rebuilds)
    start_cxx_compile_task(pool, config, options, rebuild, failed_compiles);

  pool.block_until_finished();

  if (failed_compiles.size() > 0) {
    threadsafe_print("errors in files:\n");
    failed_compiles.map(
      [](auto const& file) { threadsafe_print("\t", file, '\n'); });

    throw std::runtime_error("fatal errors when compiling cxx source files");
  }

  std::vector<std::filesystem::path> object_files;
  auto c_objs = get_files_by_type(source_filepaths, FileType::CSource);
  auto cxx_objs = get_files_by_type(source_filepaths, FileType::CXXSource);

  std::ranges::transform(c_objs, c_objs.begin(), object_file_for);
  std::ranges::transform(cxx_objs, cxx_objs.begin(), object_file_for);

  std::ranges::set_union(
    c_objs, cxx_objs, std::inserter(object_files, object_files.end()));

  object_files.push_back(hewg_builtinsym_obj_path);

  return object_files;
}
