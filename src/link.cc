#include "analysis.hh"
#include "build.hh"
#include "cmdline.hh"
#include "common.hh"
#include "confs.hh"
#include "deptree.hh"
#include "link.hh"
#include "packages.hh"
#include "paths.hh"
#include "target.hh"

#include <filesystem>

namespace
{

auto
generate_link_flags(BuildContext const&                    build,
                    std::span<std::filesystem::path const> object_files,
                    std::filesystem::path const&           outfile)
{
  std::vector<std::string> args;

  args.insert(args.end(), object_files.begin(), object_files.end());
  std::ranges::transform(args, args.begin(), [](auto const& file) {
    return std::filesystem::relative(file);
  });

  args.emplace_back("-o");
  args.emplace_back(outfile);

  if (build.target().ld != "ld")
    args.push_back(std::format("-fuse-ld={}", build.target().ld));

  // if (is_release)
  //   args.push_back("-flto");

  // TODO: add options for libraries

  return args;
}

// TODO: link libraries from packages
// this requires dependency resolvement...
auto
get_library_flags(AppContext const&     ctx,
                  PackageContext const& pkg,
                  BuildContext const&   build)
{
  std::vector<std::string> args;

  args.reserve(ctx.config().depends.system_libraries.size());
  for (auto const& sys : ctx.config().depends.system_libraries)
    args.push_back(std::format("-l{}", sys));

  auto const links = collect_packages_to_link(ctx, pkg);

  for (auto const& ident : links) {
    args.push_back(
      std::format("{}",
                  std::filesystem::canonical(
                    get_packages_static_library_file(ident, build.pic()))
                    .string()));
  }

  args.emplace_back("-L/usr/local/lib");

  return args;
}

}

void
link_executable(AppContext const&                      ctx,
                PackageContext const&                  pkg,
                BuildContext const&                    build,
                std::span<std::filesystem::path const> object_files)
{
  if (not std::filesystem::is_directory(build.emitdir()))
    throw std::runtime_error("output_directory in link() isn't a directory");

  auto const output_filepath = build.emitdir() / ctx.config().project.name;

  auto args = generate_link_flags(build, object_files, output_filepath);

  threadsafe_print("now lets get linking...\n");
  append_vec(args, get_library_flags(ctx, pkg, build));

  run_command(build.target().cxx, args);

  // we also want to strip the executable if we're
  // creating a release executable
  if (ctx.build_options().release) {
    run_command("strip", "-s", output_filepath.string());
  }
}

void
pack_static_library(AppContext const&                      ctx,
                    BuildContext const&                    build,
                    std::span<std::filesystem::path const> object_files)
{
  if (not std::filesystem::is_directory(build.emitdir()))
    throw std::runtime_error(
      "output_directory in pack_static_library() isn't a directory");

  std::filesystem::path const outfile
    = build.emitdir()
    / static_library_name_for_project(ctx.config(), build.pic());

  std::vector<std::string> commands;
  commands.emplace_back("rcs");
  commands.push_back(outfile.string());
  for (auto const& objects : object_files)
    commands.push_back(objects.string());
  run_command(build.target().ar, commands);
}

void
shared_link(AppContext const&                      ctx,
            PackageContext const&                  pkg,
            BuildContext const&                    build,
            std::span<std::filesystem::path const> object_files)
{
  if (not std::filesystem::is_directory(build.emitdir()))
    throw std::runtime_error(
      "output_directory in shared_link() isn't a directory");

  std::filesystem::path const outfile
    = build.emitdir() / std::format("lib{}.so", ctx.config().project.name);
  std::vector<std::string> args
    = generate_link_flags(build, object_files, outfile);
  append_vec(args, get_library_flags(ctx, pkg, build));
  args.emplace_back("-shared");

  run_command(build.target().cxx, args);
}
