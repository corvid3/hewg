#include <filesystem>
#include <format>
#include <fstream>
#include <jayson.hh>
#include <ostream>
#include <scl.hh>
#include <stdexcept>

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

// updates the symlink in .hewg/bin for a given package to point
// to a specified version-target
static void
select_executable(PackageCacheDB const& db,
                  PackageIdentifier const package_ident)
{
  if (not db.contains(package_ident))
    throw std::runtime_error(
      std::format("attempting to select executable {}, which doesn't exist",
                  package_ident));

  auto const package_dir = get_package_directory(package_ident);
  auto const exe_path = package_dir / package_ident.name();

  if (std::filesystem::exists(hewg_bin_directory / package_ident.name()))
    std::filesystem::remove(hewg_bin_directory / package_ident.name());

  std::filesystem::create_symlink(exe_path,
                                  hewg_bin_directory / package_ident.name());
}

static void
install_executable(ConfigurationFile const& config,
                   PackageIdentifier const& ident,
                   std::filesystem::path const& install_dir)
{
  auto const executable = get_artifact_folder(ident) / config.project.name;
  std::filesystem::copy_file(executable,
                             install_dir / config.project.name,
                             std::filesystem::copy_options::update_existing);
}

static void
install_headers(ConfigurationFile const& config,
                PackageIdentifier const&,
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
                PackageIdentifier const& this_package_ident,
                std::filesystem::path const& install_dir)
{
  install_headers(config, this_package_ident, install_dir);

  auto const lib_filename = static_library_name_for_project(config, false);
  auto const lib_pie_filename = static_library_name_for_project(config, true);

  auto const target_path =
    hewg_target_directory_path / this_package_ident.target().to_string();

  std::filesystem::copy(target_path / lib_filename,
                        install_dir / lib_filename,
                        std::filesystem::copy_options::update_existing);

  std::filesystem::copy(target_path / lib_pie_filename,
                        install_dir / lib_pie_filename,
                        std::filesystem::copy_options::update_existing);
}

/*

~/.hewg/packages
  * package directory

~/.hewg/packages/org:some-package:version:target
  * "hash" of a package, installed files and readmes are in here
  * binaries are also in here, and are symlinked from

~/.hewg/bin/
  * symlink directory for binaries, so they may appear in the path

*/

void
install(ConfigurationFile const& config,
        PackageCacheDB& db,
        TargetTriplet const target,
        BuildOptions const&)
{
  auto const package_ident = get_this_package_ident(config, target);

  ensure_user_hewg_directory();

  auto const install_directory = create_package_instance(db, package_ident);

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
      install_executable(config, package_ident, install_directory);

      // NOTE: maybe the end user doesn't want to immediately select the
      // version? also, intended to be that one could run select by itself and
      // choose the version of a package to put onto the path.
      select_executable(db, package_ident);
      break;

    case ProjectType::StaticLibrary:
      install_library(config, package_ident, install_directory);
      break;

    case ProjectType::SharedLibrary:
      throw std::runtime_error(
        "hewg does not support installing shared libraries");
      break;

    case ProjectType::Headers:
      install_headers(config, package_ident, install_directory);
      break;
  }

  save_package_db(db);
}
