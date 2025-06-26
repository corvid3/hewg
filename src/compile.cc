#include <algorithm>
#include <chrono>
#include <expected>
#include <filesystem>
#include <format>
#include <future>
#include <iterator>
#include <jayson.hh>
#include <optional>
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
  return generate_common_flags(is_release, PIC) + config.c.flags +
         std::vector{ std::format(
           "-std={}", get_c_standard_string(config.c.std.value_or(17))) };
};

constexpr auto generate_cxx_flags =
  [](ConfigurationFile const& config,
     bool const is_release,
     bool const PIC) static -> std::vector<std::string> {
  return generate_common_flags(is_release, PIC) + config.cxx.flags +
         std::vector{ std::format(
           "-std={}", get_cxx_standard_string(config.cxx.std.value_or(20))) };
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
static std::future<std::optional<std::string>>
start_cxx_compile_task(ThreadPool& thread_pool,
                       ConfigurationFile const&,
                       ToolFile const& tool_file,
                       std::filesystem::path const source_filepath,
                       std::filesystem::path const object_filepath,
                       std::filesystem::path const depend_filepath,
                       std::vector<std::string> common_flags)
{
  return thread_pool.add_job([source_filepath,
                              object_filepath,
                              depend_filepath,
                              common_flags,
                              &tool_file,
                              &thread_pool]() -> std::optional<std::string> {
    auto const relative_source_path =
      std::filesystem::relative(source_filepath, hewg_cxx_src_directory_path);

    threadsafe_print(
      std::format("compiling CXX file: <{}>\n", relative_source_path.string()));

    auto const [exit_code, what] = run_command(
      tool_file.cxx,
      common_flags +
        generate_file_flags(source_filepath, depend_filepath, object_filepath));

    if (exit_code != 0) {
      thread_pool.drain();
      return relative_source_path.string();
      // write_error_file(source_filepath, what);
    }

    return std::nullopt;
  });
}

static std::future<std::optional<std::string>>
start_c_compile_task(ThreadPool& thread_pool,
                     ConfigurationFile const&,
                     ToolFile const& tool_file,
                     std::filesystem::path const source_filepath,
                     std::filesystem::path const object_filepath,
                     std::filesystem::path const depend_filepath,
                     std::vector<std::string> common_flags)
{
  return thread_pool.add_job([source_filepath,
                              object_filepath,
                              depend_filepath,
                              common_flags,
                              &tool_file,
                              &thread_pool]() -> std::optional<std::string> {
    auto const relative_source_path =
      std::filesystem::relative(source_filepath, hewg_c_src_directory_path);

    threadsafe_print(
      std::format("compiling C file: <{}>\n", relative_source_path.string()));

    auto const [exit_code, what] = run_command(
      tool_file.cc,
      common_flags +
        generate_file_flags(source_filepath, depend_filepath, object_filepath));

    if (exit_code != 0) {
      thread_pool.drain();
      return relative_source_path.string();
      // write_error_file(source_filepath, what);
    }

    return std::nullopt;
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

std::filesystem::path
compile_hewgsym(ConfigurationFile const& config,
                ToolFile const& tools,
                bool PIC)
{
  auto const object_file_name =
    PIC ? hewg_builtinsym_obj_pic_path : hewg_builtinsym_obj_path;

  std::ofstream(hewg_builtinsym_src_path)
    << emit_symcache_contents(config.project.name, config.project.version);

  std::vector<std::string> args;
  args.push_back("-O2");
  args.push_back("-c");
  args.push_back(hewg_builtinsym_src_path);
  args.push_back("-o");
  args.push_back(object_file_name);
  if (PIC)
    args.push_back("-fPIC");

  run_command(tools.cc, args);

  return object_file_name;
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

// starts compilation for cxx
// without blocking
// make sure you synchronize the threads after this!
std::pair<std::vector<std::filesystem::path>,
          std::vector<std::future<std::optional<std::string>>>>
compile_cxx(ThreadPool& threads,
            ConfigurationFile const& config,
            ToolFile const& tools,
            std::filesystem::path const& cache_folder,
            bool const release,
            bool const PIC)
{
  auto const cxx_filepaths = get_cxx_source_filepaths(config);
  std::vector<std::filesystem::path> cxx_objects;
  std::ranges::transform(cxx_filepaths,
                         std::inserter(cxx_objects, cxx_objects.end()),
                         [=](std::filesystem::path const& path) {
                           return object_file_for_cxx(cache_folder, path);
                         });

  ensure_object_output_paths_exist(cxx_objects);

  auto const cxx_rebuilds =
    mark_cxx_files_for_rebuild(cache_folder, cxx_filepaths);
  auto const cxx_flags = generate_cxx_flags(config, release, PIC);

  {
    std::string cxx_flags_fmt;
    std::ranges::for_each(cxx_flags, [&](std::string_view in) {
      cxx_flags_fmt += in, cxx_flags_fmt += ' ';
    });

    threadsafe_print_verbose(std::format("CXX flags: {}", cxx_flags_fmt));
  }

  std::vector<std::future<std::optional<std::string>>> awaits;

  for (auto const& rebuild : cxx_rebuilds) {
    auto const object_filepath = object_file_for_cxx(cache_folder, rebuild);
    auto const depend_filepath = depfile_for_cxx(cache_folder, rebuild);

    awaits.push_back(start_cxx_compile_task(threads,
                                            config,
                                            tools,
                                            rebuild,
                                            object_filepath,
                                            depend_filepath,
                                            cxx_flags));
  }

  return std::pair{ cxx_objects, std::move(awaits) };
}

std::pair<std::vector<std::filesystem::path>,
          std::vector<std::future<std::optional<std::string>>>>
compile_c(ThreadPool& threads,
          ConfigurationFile const& config,
          ToolFile const& tools,
          std::filesystem::path const& cache_folder,
          bool const release,
          bool const PIC)
{
  auto const c_filepaths = get_c_source_filepaths(config);
  std::vector<std::filesystem::path> c_objects;

  std::ranges::transform(c_filepaths,
                         std::inserter(c_objects, c_objects.end()),
                         [=](std::filesystem::path const& path) {
                           return object_file_for_c(cache_folder, path);
                         });

  ensure_object_output_paths_exist(c_objects);

  auto const c_rebuilds = mark_c_files_for_rebuild(cache_folder, c_filepaths);
  auto c_flags = generate_c_flags(config, release, PIC);

  {
    std::string c_flags_fmt;
    std::ranges::for_each(c_flags, [&](std::string_view in) {
      c_flags_fmt += in, c_flags_fmt += ' ';
    });

    threadsafe_print_verbose(std::format("C flags: {}", c_flags_fmt));
  }

  std::vector<std::future<std::optional<std::string>>> awaits;

  for (auto const& rebuild : c_rebuilds) {
    auto const object_file = object_file_for_c(cache_folder, rebuild);
    auto const depend_file = depfile_for_c(cache_folder, rebuild);

    awaits.push_back(start_c_compile_task(
      threads, config, tools, rebuild, object_file, depend_file, c_flags));
  }

  return { c_objects, std::move(awaits) };
}
