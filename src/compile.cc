#include <algorithm>
#include <chrono>
#include <cmath>
#include <crow.jayson/jayson.hh>
#include <filesystem>
#include <format>
#include <future>
#include <iterator>
#include <memory>
#include <optional>
#include <print>
#include <span>

#include "analysis.hh"
#include "common.hh"
#include "compile.hh"
#include "confs.hh"
#include "packages.hh"
#include "paths.hh"
#include "semver.hh"
#include "thread_pool.hh"

std::vector<std::string>
generate_c_cxx_file_flags(std::filesystem::path const filepath,
                          std::filesystem::path const depfile,
                          std::filesystem::path const object_file)
{
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
  [](PackageIdentifier const& this_ident,
     bool const is_release,
     bool const PIC) static -> std::vector<std::string> {
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
    copy = copy + std::vector<std::string>{ "-O0", "-g" };

  if (PIC)
    copy = copy + std::vector<std::string>{ "-fPIC" };

  copy = copy + std::vector<std::string>{ std::format("-DHEWG_ORG={}",
                                                      this_ident.org()) };

  copy = copy + std::vector<std::string>{ std::format("-DHEWG_NAME={}",
                                                      this_ident.name()) };

  return copy;
};

std::vector<std::string>
generate_c_flags(std::span<std::string const> user_flags,
                 std::span<std::filesystem::path const> include_dirs,
                 int const std,
                 PackageIdentifier const& ident,
                 bool const is_release,
                 bool const PIC)
{
  auto out = generate_common_flags(ident, is_release, PIC) +
             std::vector(user_flags.begin(), user_flags.end()) +
             std::vector{ std::format("-std={}", get_c_standard_string(std)) };

  for (auto const& dir : include_dirs)
    out.push_back(std::format("-I{}", std::filesystem::absolute(dir).string()));

  return out;
};

std::vector<std::string>
generate_cxx_flags(std::span<std::string const> user_flags,
                   std::span<std::filesystem::path const> include_dirs,
                   int const std,
                   PackageIdentifier const& ident,
                   bool const is_release,
                   bool const PIC)
{
  auto out =
    generate_common_flags(ident, is_release, PIC) +
    std::vector(user_flags.begin(), user_flags.end()) +
    std::vector{ std::format("-std={}", get_cxx_standard_string(std)) };

  for (auto const& dir : include_dirs)
    out.push_back(std::format("-I{}", std::filesystem::absolute(dir).string()));

  return out;
};

struct format_data
{
  int total_num;
  int num_digits;

  std::atomic<int> counter;
};

static void
print_status(format_data& formatting,
             std::string_view const language,
             std::filesystem::path const file)
{
  auto const pct = formatting.counter / (float)formatting.total_num * 0.5 + 0.4;

  threadsafe_print(
    std::format("({}{:{}}\x1b[39m/\x1b[38;2;230;230;230m{:{}}\x1b[39m) "
                "[\x1b[2m{}\x1b[22m] {}\n",
                greyscale_terminal_colorize(pct),
                formatting.counter++,
                formatting.num_digits,
                formatting.total_num,
                formatting.num_digits,
                language,
                file.string()));
}

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
                       TargetFile const& tool_file,
                       CXXSourceRelatives const& relatives,
                       CXXSourceRelatives::File const& file,
                       std::vector<std::string> common_flags,
                       std::shared_ptr<format_data> formatting)
{
  return thread_pool.add_job(
    [common_flags, formatting, &tool_file, &thread_pool, &relatives, &file]()
      -> std::optional<std::string> {
      print_status(*formatting, "CXX", file.source);

      auto const [exit_code, what] = run_command(
        tool_file.cxx,
        common_flags +
          generate_c_cxx_file_flags(relatives.source_directory() / file.source,
                                    relatives.cache_directory() / file.depends,
                                    relatives.cache_directory() / file.object));

      if (exit_code != 0) {
        thread_pool.drain();
        return file.source.string();
        // write_error_file(source_filepath, what);
      }

      return std::nullopt;
    });
}

static std::future<std::optional<std::string>>
start_c_compile_task(ThreadPool& thread_pool,
                     TargetFile const& tool_file,
                     CSourceRelatives const& relatives,
                     CSourceRelatives::File const& file,
                     std::vector<std::string> common_flags,
                     std::shared_ptr<format_data> formatting)
{
  return thread_pool.add_job(
    [&relatives, &file, common_flags, formatting, &tool_file, &thread_pool]()
      -> std::optional<std::string> {
      print_status(*formatting, "C", file.source);

      auto const [exit_code, what] = run_command(
        tool_file.cc,
        common_flags +
          generate_c_cxx_file_flags(relatives.source_directory() / file.source,
                                    relatives.cache_directory() / file.depends,
                                    relatives.cache_directory() / file.object));

      if (exit_code != 0) {
        thread_pool.drain();
        return file.source.string();
        // write_error_file(source_filepath, what);
      }

      return std::nullopt;
    });
}

static std::string
emit_symcache_contents(PackageType const this_package_type,
                       PackageIdentifier const& ident)
{
  auto const& version = ident.version();

  std::string out;

  out += std::format("int __hewg_{}_{}_version[3] = {{ {}, {}, {} }};",
                     ident.org(),
                     ident.name(),
                     version.major(),
                     version.minor(),
                     version.patch());

  out += "\n";

  if (auto const pre = version.prerelease())
    out += std::format("char const* __hewg_{}_{}_prerelease = \"{}\";",
                       ident.org(),
                       ident.name(),
                       *pre);
  else
    out += std::format("char const* __hewg_{}_{}_prerelease = (char const*)0;",
                       ident.org(),
                       ident.name());

  out += "\n";

  if (auto const meta = version.metadata())
    out += std::format("char const* __hewg_{}_{}_metadata = \"{}\";",
                       ident.org(),
                       ident.name(),
                       *meta);
  else
    out += std::format("char const* __hewg_{}_{}_metadata = (char const*)0;",
                       ident.org(),
                       ident.name());

  out += "\n";

  out += std::format("char const* __hewg_{}_{}_build_target_triplet[3] = {{ "
                     "\"{}\", \"{}\", \"{}\" }};",
                     ident.org(),
                     ident.name(),
                     ident.target().architecture(),
                     ident.target().os(),
                     ident.target().vendor());

  out += "\n";

  if (this_package_type == PackageType::Executable) {
    // TODO: hewg bundling, later
    // this is just set up in advance so i can eventually set up bundling
    out += std::format("int const __hewg_bundled = 1;");
    out += "\n";
  }

  using namespace std::chrono;

  auto const now = duration_cast<seconds>(utc_clock::now().time_since_epoch());

  out += std::format("long __hewg_{}_{}_build_date = {};",
                     ident.org(),
                     ident.name(),
                     now.count());

  out += "\n";

  return out;
}

std::filesystem::path
compile_hewgsym(ConfigurationFile const& conf,
                TargetFile const& tools,
                PackageIdentifier const& this_ident,
                bool PIC)
{
  auto const object_file_name =
    PIC ? hewg_builtinsym_obj_pic_path : hewg_builtinsym_obj_path;

  std::ofstream(hewg_builtinsym_src_path)
    << emit_symcache_contents(conf.meta.type, this_ident);

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
std::vector<std::future<std::optional<std::string>>>
compile_cxx(ThreadPool& threads,
            std::span<std::string const> flags,
            CXXSourceRelatives const& src,
            TargetFile const& tools)
{
  {
    for (auto const& path : src.files())
      std::filesystem::create_directories(path.object.parent_path()),
        std::filesystem::create_directories(path.depends.parent_path());
  }

  auto const cxx_rebuilds = mark_cxx_files_for_rebuild(src);

  {
    std::string cxx_flags_fmt;
    std::ranges::for_each(flags, [&](std::string_view in) {
      cxx_flags_fmt += in, cxx_flags_fmt += ' ';
    });

    threadsafe_print_verbose(std::format("CXX flags: {}", cxx_flags_fmt));
  }

  auto const num = cxx_rebuilds.size();
  auto const digits = std::ceil(std::log10(num));
  std::shared_ptr<format_data> formatting(new format_data);
  formatting->counter = 1;
  formatting->total_num = num;
  formatting->num_digits = digits;

  std::vector<std::future<std::optional<std::string>>> awaits;

  for (auto const& rebuild : cxx_rebuilds) {
    awaits.push_back(start_cxx_compile_task(
      threads,
      tools,
      src,
      rebuild.get(),
      std::vector<std::string>{ flags.begin(), flags.end() },
      formatting));
  }

  return awaits;
}

std::vector<std::future<std::optional<std::string>>>
compile_c(ThreadPool& threads,
          std::span<std::string const> flags,
          CSourceRelatives const& src,
          TargetFile const& tools)
{
  {
    for (auto const& path : src.files())
      std::filesystem::create_directories(path.object.parent_path()),
        std::filesystem::create_directories(path.depends.parent_path());
  }

  auto const c_rebuilds = mark_c_files_for_rebuild(src);

  {
    std::string c_flags_fmt;
    std::ranges::for_each(flags, [&](std::string_view in) {
      c_flags_fmt += in, c_flags_fmt += ' ';
    });

    threadsafe_print_verbose(std::format("C flags: {}", c_flags_fmt));
  }

  auto const num = c_rebuilds.size();
  auto const digits = std::ceil(std::log10(num));
  std::shared_ptr<format_data> formatting(new format_data);
  formatting->counter = 1;
  formatting->total_num = num;
  formatting->num_digits = digits;

  std::vector<std::future<std::optional<std::string>>> awaits;

  for (auto const& rebuild : c_rebuilds) {
    awaits.push_back(start_c_compile_task(
      threads,
      tools,
      src,
      rebuild,
      std::vector<std::string>{ flags.begin(), flags.end() },
      formatting));
  }

  return awaits;
}
