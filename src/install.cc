#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <jayson.hh>
#include <ostream>
#include <scl.hh>
#include <stdexcept>
#include <vector>

#include "analysis.hh"
#include "common.hh"
#include "confs.hh"
#include "install.hh"
#include "packages.hh"
#include "paths.hh"

/*

  since multiple versions of a binary
  can be installed at once, it's a smart idea
  to have a symlink to the binary in just a /bin
  directory.

  this also means there needs to be a way to select
  and change the currently used version of the binary,
  to swap out the symlink to point to another version

*/

// returns the path to the user hewg directory
static std::filesystem::path
ensure_user_hewg_directory()
{
  create_directory_checked(user_hewg_directory);
  create_directory_checked(hewg_packages_directory);
  create_directory_checked(hewg_bin_directory);

  return user_hewg_directory;
}

static jayson::val
skel_manifest(std::string_view package_name)
{
  jayson::obj obj;
  obj.insert_or_assign("name", jayson::val(std::string(package_name)));
  obj.insert_or_assign("versions", jayson::array{});
  return obj;
}

static void
ensure_package(std::string_view package_name)
{
  auto const package_dir = hewg_packages_directory / package_name;

  std::filesystem::create_directories(package_dir);

  if (not std::filesystem::exists(package_dir / "manifest.json")) {
    std::ofstream(package_dir / "manifest.json")
      << skel_manifest(package_name).serialize(false);
  }
}

static void
select_executable(std::string_view name, version_triplet const trip)
{
  VersionManifest manifest = open_version_manifest(name);

  auto const version = std::ranges::find(manifest.versions, trip);
  if (version == manifest.versions.end())
    throw std::runtime_error(
      std::format("unable to select executable version {} for package {}, as "
                  "it is not available",
                  version_triplet_to_string(trip),
                  name));

  auto const package_dir =
    hewg_packages_directory / name / version_triplet_to_string(trip);
  auto const exec_path = package_dir / name;

  if (std::filesystem::exists(hewg_bin_directory / name)) {
    if (not std::filesystem::is_symlink(hewg_bin_directory / name))
      throw std::runtime_error(
        std::format("{} exists, but isn't a symlink??? try deleting it.",
                    (hewg_bin_directory / name).string()));

    threadsafe_print(std::format("deleting symlink {}...\n",
                                 (hewg_bin_directory / name).string()));
    do_terminal_countdown(3);

    std::filesystem::remove(hewg_bin_directory / name);
  }

  std::filesystem::create_symlink(exec_path, hewg_bin_directory / name);
}

static void
install_executable(ConfigurationFile const& config,
                   std::string_view profile,
                   std::filesystem::path const& install_dir)
{
  auto const executable_name = config.project.name;
  auto const executable_path =
    get_target_folder_for_build_profile(profile) / executable_name;

  std::filesystem::copy_file(executable_path,
                             install_dir / executable_name,
                             std::filesystem::copy_options::update_existing);

  select_executable(executable_name, config.project.version);
}

static void
install_headers(ConfigurationFile const& config,
                std::string_view,
                std::filesystem::path const& install_dir)
{
  auto const include_header_dir = install_dir / "include" / config.project.name;
  std::filesystem::create_directories(install_dir / "include");
  std::filesystem::copy(hewg_public_header_directory_path,
                        include_header_dir,
                        std::filesystem::copy_options::recursive |
                          std::filesystem::copy_options::update_existing);
}

static void
install_library(ConfigurationFile const& config,
                std::string_view profile,
                std::filesystem::path const& install_dir)
{
  install_headers(config, profile, install_dir);

  auto const lib_filename = static_library_name_for_project(config, false);
  auto const lib_pie_filename = static_library_name_for_project(config, true);

  std::filesystem::copy(get_target_folder_for_build_profile(profile) /
                          lib_filename,
                        install_dir / lib_filename,
                        std::filesystem::copy_options::update_existing);

  std::filesystem::copy(get_target_folder_for_build_profile(profile) /
                          lib_pie_filename,
                        install_dir / lib_pie_filename,
                        std::filesystem::copy_options::update_existing);
}

void
install(ConfigurationFile const& config,
        InstallOptions const&,
        std::string_view profile)
{
  ensure_user_hewg_directory();
  ensure_package(config.project.name);
  auto const install_directory =
    add_version_to_package(config.project.name, config.project.version);

  {
    auto const info_path = install_directory / "info.scl";

    // we want just the meta & project information
    scl::file file;
    scl::serialize(config.meta, file, "hewg");
    scl::serialize(config.project, file, "project");

    std::ofstream(info_path) << file.serialize();
  }

  switch (config.meta.type) {
    case ProjectType::Executable:
      install_executable(config, profile, install_directory);
      break;

    case ProjectType::StaticLibrary:
      install_library(config, profile, install_directory);
      break;

    case ProjectType::SharedLibrary:
      throw std::runtime_error(
        "hewg does not support installing shared libraries");
      break;

    case ProjectType::Headers:
      install_headers(config, profile, install_directory);
      break;
  }
}
