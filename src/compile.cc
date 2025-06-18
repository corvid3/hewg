#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <iterator>
#include <jayson.hh>
#include <span>

#include "analysis.hh"
#include "common.hh"
#include "compile.hh"
#include "confs.hh"
#include "paths.hh"
#include "thread_pool.hh"

constexpr auto generate_file_flags =
  [](std::filesystem::path const filepath,
     std::filesystem::path const depfile,
     std::filesystem::path const object_file) static
  -> std::vector<std::string> {
  using std::filesystem::relative;

  return {
    "-MMD",
    "-MF",
    relative(depfile),
    "-o",
    relative(object_file),
    relative(filepath),
  };
};

constexpr auto generate_common_flags =
  [](bool const is_release, bool const PIC) static -> std::vector<std::string> {
  static const std::vector<std::string> common_flags = {
    "-c",
    "-Iprivate",
    "-Iinclude",
    "-fdiagnostics-color=always",
  };

  auto copy = common_flags;

  if (is_release)
    copy = copy + std::vector<std::string>{ "-O2" };
  else
    copy = copy + std::vector<std::string>{ "-Og", "-g" };

  if (PIC)
    copy = copy + std::vector<std::string>{ "-fPIC" };

  return copy;
};

constexpr auto generate_c_flags =
  [](ConfigurationFile const& config,
     bool const is_release,
     bool const PIC) static -> std::vector<std::string> {
  return generate_common_flags(is_release, PIC) + config.flags.c_flags;
};

constexpr auto generate_cxx_flags =
  [](ConfigurationFile const& config,
     bool const is_release,
     bool const PIC) static -> std::vector<std::string> {
  return generate_common_flags(is_release, PIC) + config.flags.cxx_flags;
};

// static void
// write_error_file(std::filesystem::path src_file, std::string_view what)
// {
//   auto const relative =
//     std::filesystem::relative(src_file, hewg_src_directory_path);
//   auto const file = hewg_err_directory_path / relative;

//   std::filesystem::create_directories(file.parent_path());
//   std::ofstream(file) << what;

//   // strip the output file of console codes with sed,
//   // if there's any
//   std::vector<std::string> sed_args;
//   sed_args.push_back(R"(s/\x1B\[[0-9;]*[mKG]//g)");
//   sed_args.push_back(file);
//   sed_args.push_back("-i");

//   run_command("sed", sed_args);
// }

// will set successful_compile to false
// if the task failed
static void
start_cxx_compile_task(ThreadPool& thread_pool,
                       ConfigurationFile const& config,
                       std::filesystem::path const source_filepath,
                       std::vector<std::string> args,
                       atomic_vec<std::string>& failed_compiles)
{
  thread_pool.add_job(
    [source_filepath, args, &config, &thread_pool, &failed_compiles]() {
      auto const relative_path =
        std::filesystem::relative(source_filepath, hewg_cxx_src_directory_path);

      threadsafe_print(
        std::format("compiling CXX file: <{}>\n", relative_path.string()));

      auto const [exit_code, what] =
        run_command(config.tools.cxx_tool.value_or("c++"), args);

      if (exit_code != 0) {
        failed_compiles.push_back(relative_path);
        thread_pool.drain();
        // write_error_file(source_filepath, what);
      }
    });
}

static void
start_c_compile_task(ThreadPool& thread_pool,
                     ConfigurationFile const& config,
                     std::filesystem::path const source_filepath,
                     std::vector<std::string> args,
                     atomic_vec<std::string>& failed_compiles)
{
  thread_pool.add_job(
    [source_filepath, args, &config, &thread_pool, &failed_compiles]() {
      auto const relative_path =
        std::filesystem::relative(source_filepath, hewg_c_src_directory_path);

      threadsafe_print(
        std::format("compiling C file: <{}>\n", relative_path.string()));

      auto const [exit_code, what] =
        run_command(config.tools.c_tool.value_or("cc"), args);

      if (exit_code != 0) {
        failed_compiles.push_back(relative_path);
        thread_pool.drain();
        // write_error_file(source_filepath, what);
      }
    });
}

static std::string
emit_symcache_contents(std::string_view package_name,
                       version_triplet const trip)
{
  auto const [x, y, z] = trip;

  std::string out;

  out += std::format("int __hewg_version_package_{}[3] = {{ {}, {}, {} }};\n",
                     package_name,
                     x,
                     y,
                     z);

  using namespace std::chrono;

  auto const now = duration_cast<seconds>(utc_clock::now().time_since_epoch());

  out += std::format(
    "long __hewg_build_date_package_{} = {};\n", package_name, now.count());

  return out;
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

static auto
ensure_symcache(ConfigurationFile const& config)
{
  if (check_symcache(config)) {
    run_command("cc",
                hewg_builtinsym_src_path.string(),
                "-O2",
                "-c",
                "-o",
                hewg_builtinsym_obj_path.string());
  }
}

static auto
ensure_object_output_paths_exist(
  std::span<std::filesystem::path const> object_filepaths)
{
  for (auto const& path : object_filepaths)
    std::filesystem::create_directories(path.parent_path());
}

/*
  for executables,
  just compile the object files once,
  not PIC

  for static libraries,
  compile the object files twice
  one without PIC, one with PIC

  for dynamic libraries,
  compile the object files once,
  just with PIC
*/

std::vector<std::filesystem::path>
compile_c_cxx(ThreadPool& pool,
              ConfigurationFile const& config,
              bool const release,
              bool const PIC)
{
  auto const c_filepaths = get_c_source_filepaths(config);
  auto const cxx_filepaths = get_cxx_source_filepaths(config);

  auto const object_files = ({
    std::vector<std::filesystem::path> c_objects;
    std::vector<std::filesystem::path> cxx_objects;

    std::ranges::transform(c_filepaths,
                           std::inserter(c_objects, c_objects.end()),
                           [=](std::filesystem::path const& path) {
                             return object_file_for_c(path, PIC);
                           });

    std::ranges::transform(cxx_filepaths,
                           std::inserter(cxx_objects, cxx_objects.end()),
                           [=](std::filesystem::path const& path) {
                             return object_file_for_cxx(path, PIC);
                           });

    std::vector<std::filesystem::path> objects;
    std::ranges::set_union(
      c_objects, cxx_objects, std::inserter(objects, objects.end()));

    objects.push_back(hewg_builtinsym_obj_path);

    objects;
  });

  auto const c_rebuilds = mark_c_files_for_rebuild(c_filepaths, PIC);
  auto const cxx_rebuilds = mark_cxx_files_for_rebuild(cxx_filepaths, PIC);

  auto const c_flags = generate_c_flags(config, release, PIC);
  auto const cxx_flags = generate_cxx_flags(config, release, PIC);

  atomic_vec<std::string> failed_compiles;

  {
    std::string cxx_flags_fmt;
    std::ranges::for_each(cxx_flags, [&](std::string_view in) {
      cxx_flags_fmt += in, cxx_flags_fmt += ' ';
    });

    std::string c_flags_fmt;
    std::ranges::for_each(c_flags, [&](std::string_view in) {
      c_flags_fmt += in, c_flags_fmt += ' ';
    });

    threadsafe_print_verbose(std::format("CXX flags: {}", cxx_flags_fmt));
    threadsafe_print_verbose(std::format("C flags: {}", c_flags_fmt));
  }

  ensure_symcache(config);
  ensure_object_output_paths_exist(object_files);

  {
    for (auto const& rebuild : c_rebuilds)
      start_c_compile_task(
        pool,
        config,
        rebuild,
        c_flags + generate_file_flags(rebuild,
                                      depfile_for_c(rebuild, PIC),
                                      object_file_for_c(rebuild, PIC)),
        failed_compiles);

    for (auto const& rebuild : cxx_rebuilds)
      start_cxx_compile_task(
        pool,
        config,
        rebuild,
        cxx_flags + generate_file_flags(rebuild,
                                        depfile_for_cxx(rebuild, PIC),
                                        object_file_for_cxx(rebuild, PIC)),
        failed_compiles);

    pool.block_until_finished();
  }

  if (failed_compiles.size() > 0) {
    threadsafe_print("errors in files:\n");
    failed_compiles.map(
      [](auto const& file) { threadsafe_print("\t", file, '\n'); });

    throw std::runtime_error("fatal errors when compiling cxx source files");
  }

  return object_files;
}
