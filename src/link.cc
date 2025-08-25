
#include "analysis.hh"
#include "cmdline.hh"
#include "common.hh"
#include "confs.hh"
#include "packages.hh"
#include "target.hh"
#include <filesystem>

static auto
generate_link_flags(ConfigurationFile const&,
                    TargetFile const& target,
                    bool const,
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

  if (target.ld != "ld")
    args.push_back(std::format("-fuse-ld={}", target.ld));

  // if (is_release)
  //   args.push_back("-flto");

  // TODO: add options for libraries

  return args;
}

// TODO: link libraries from packages
// this requires dependency resolvement...
static auto
get_library_flags(ConfigurationFile const& config,
                  TargetFile const& target,
                  PackageCacheDB const& db,
                  Deptree const& deptree,
                  bool const PIE)
{
  std::vector<std::string> args;

  auto const links =
    collect_packages_to_link(config, db, target.triplet, deptree);

  for (auto const& ident : links) {
    args.push_back(std::format(
      "{}",
      std::filesystem::canonical(get_packages_static_library_file(ident, PIE))
        .string()));
  }

  args.push_back("-L/usr/local/lib");

  for (auto const& native_library : config.libs.native)
    args.push_back(std::format("-l{}", native_library));

  // for (auto const& packaged_library : config.libs.packages) {
  //   (void)packaged_library;
  // }

  return args;
}

void
link_executable(ConfigurationFile const& config,
                TargetFile const& target,
                BuildOptions const& options,
                PackageCacheDB const& db,
                Deptree const& deptree,
                std::span<std::filesystem::path const> object_files,
                std::filesystem::path output_directory)
{
  if (not std::filesystem::is_directory(output_directory))
    throw std::runtime_error("output_directory in link() isn't a directory");

  auto const output_filepath = output_directory / config.project.name;

  auto args = generate_link_flags(
    config, target, options.release, object_files, output_filepath);

  threadsafe_print("now lets get linking...\n");
  append_vec(args, get_library_flags(config, target, db, deptree, false));

  run_command(target.cxx, args);

  // we also want to strip the executable if we're
  // creating a release executable
  if (options.release) {
    run_command("strip", "-s", output_filepath.string());
  }
}

void
pack_static_library(ConfigurationFile const& config,
                    TargetFile const& target,
                    std::span<std::filesystem::path const> object_files,
                    std::filesystem::path output_directory,
                    bool const PIC)
{
  if (not std::filesystem::is_directory(output_directory))
    throw std::runtime_error(
      "output_directory in pack_static_library() isn't a directory");

  std::filesystem::path const outfile =
    output_directory / static_library_name_for_project(config, PIC);

  std::vector<std::string> commands;
  commands.push_back("rcs");
  commands.push_back(outfile.string());
  for (auto const& objects : object_files)
    commands.push_back(objects.string());
  run_command(target.ar, commands);
}

void
shared_link(ConfigurationFile const& config,
            TargetFile const& target,
            BuildOptions const& options,
            PackageCacheDB const& db,
            Deptree const& deptree,
            std::span<std::filesystem::path const> object_files,
            std::filesystem::path output_directory)
{
  if (not std::filesystem::is_directory(output_directory))
    throw std::runtime_error(
      "output_directory in shared_link() isn't a directory");

  std::filesystem::path const outfile =
    output_directory / std::format("lib{}.so", config.project.name);

  std::vector<std::string> args =
    generate_link_flags(config, target, options.release, object_files, outfile);

  // ignore static libraries here
  // and defer their linking by adding them to the
  // descriptor of the package file
  append_vec(args, get_library_flags(config, target, db, deptree, true));

  args.push_back("-shared");

  run_command(target.cxx, args);
}
