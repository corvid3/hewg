#include "analysis.hh"
#include "common.hh"
#include "confs.hh"
#include "install.hh"
#include "packages.hh"
#include "paths.hh"

#include <crow.jayson/jayson.hh>
#include <crow.scl/scl.hh>
#include <filesystem>
#include <format>
#include <fstream>
#include <ostream>
#include <stdexcept>

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
auto
ensure_user_hewg_directory() -> std::filesystem::path
{
  create_directory_checked(user_hewg_directory);
  create_directory_checked(hewg_packages_directory);
  create_directory_checked(hewg_bin_directory);

  return user_hewg_directory;
}

// updates the symlink in .hewg/bin for a given package to point
// to a specified version-target
void
select_executable(PackageCacheDB const&   db,
                  PackageIdentifier const package_ident)
{
  if (not db.contains(package_ident)) {
    throw std::runtime_error(
      std::format("attempting to select executable {}, which doesn't exist",
                  package_ident));
  }

  auto const package_dir = get_package_directory(package_ident);
  auto const exe_path    = package_dir / package_ident.name();

  if (std::filesystem::exists(hewg_bin_directory / package_ident.name()))
    std::filesystem::remove(hewg_bin_directory / package_ident.name());

  std::filesystem::create_symlink(exe_path,
                                  hewg_bin_directory / package_ident.name());
}

static void
install_executable(ConfigurationFile const&     config,
                   PackageIdentifier const&     ident,
                   std::filesystem::path const& install_dir)
{
  auto const executable = get_artifact_folder(ident) / config.project.name;
  std::filesystem::copy_file(executable,
                             install_dir / config.project.name,
                             std::filesystem::copy_options::update_existing);
}

static void
install_headers(ConfigurationFile const&,
                PackageIdentifier const&     this_package_ident,
                std::filesystem::path const& install_dir)
{
  auto const include_header_dir
    = install_dir / "include"
    / std::format("{}.{}", this_package_ident.org(), this_package_ident.name());

  std::filesystem::create_directories(install_dir / "include");

  if (std::filesystem::exists(include_header_dir)) {
    for (auto const& file :
         std::filesystem::recursive_directory_iterator(include_header_dir)) {
      if (std::filesystem::is_regular_file(file)) {
        std::filesystem::permissions(file,
                                     std::filesystem::perms::all,
                                     std::filesystem::perm_options::remove);
        std::filesystem::permissions(file,
                                     std::filesystem::perms::owner_write
                                       | std::filesystem::perms::group_write
                                       | std::filesystem::perms::others_write,
                                     std::filesystem::perm_options::add);
      }
    }
  }

  std::filesystem::copy(hewg_public_header_directory_path,
                        include_header_dir,
                        std::filesystem::copy_options::recursive
                          | std::filesystem::copy_options::update_existing);

  if (std::filesystem::exists(include_header_dir)) {
    for (auto const& file :
         std::filesystem::recursive_directory_iterator(include_header_dir)) {
      if (std::filesystem::is_regular_file(file)) {
        std::filesystem::permissions(file,
                                     std::filesystem::perms::all,
                                     std::filesystem::perm_options::remove);
        std::filesystem::permissions(file,
                                     std::filesystem::perms::owner_read
                                       | std::filesystem::perms::group_read
                                       | std::filesystem::perms::others_read,
                                     std::filesystem::perm_options::add);
      }
    }
  }
}

static void
install_library(ConfigurationFile const&     config,
                PackageIdentifier const&     this_package_ident,
                std::filesystem::path const& install_dir)
{
  install_headers(config, this_package_ident, install_dir);

  auto const lib_filename     = static_library_name_for_project(config, false);
  auto const lib_pie_filename = static_library_name_for_project(config, true);

  auto const target_path
    = hewg_target_directory_path / this_package_ident.target().to_string();

  std::filesystem::copy(target_path / lib_filename,
                        install_dir / lib_filename,
                        std::filesystem::copy_options::update_existing);

  std::filesystem::copy(target_path / lib_pie_filename,
                        install_dir / lib_pie_filename,
                        std::filesystem::copy_options::update_existing);
}

// static void
// install_shared(ConfigurationFile const& config,
//                PackageIdentifier const& this_package_ident,
//                std::filesystem::path const& install_dir)
// {
//   install_headers(config, this_package_ident, install_dir);
//   auto const dynlib_filename =
// }

void
install(AppContext const& ctx, PackageContext const& pkg)
{
  auto const package_ident
    = get_this_package_ident(ctx.config(), ctx.triplet());

  ensure_user_hewg_directory();

  auto const install_directory
    = create_package_instance(pkg.db(), package_ident);

  {
    auto const info_path = install_directory / "manifest.json";

    std::set<DependencyIdentifier> internal;
    std::set<DependencyIdentifier> external;

    std::ranges::transform(ctx.config().depends.internal,
                           std::inserter(internal, internal.begin()),
                           [](std::string_view in) static {
                             auto const out = parse_dependency_identifier(in);

                             if (not out)
                               throw std::runtime_error("how did we get here");

                             return *out;
                           });

    std::ranges::transform(ctx.config().depends.external,
                           std::inserter(external, external.begin()),
                           [](std::string_view in) static {
                             auto const out = parse_dependency_identifier(in);

                             if (not out)
                               throw std::runtime_error("how did we get here");

                             return *out;
                           });

    PackageInfo info{ .this_identifier       = package_ident,
                      .type                  = ctx.config().meta.type,
                      .internal_dependencies = internal,
                      .external_dependencies = external };

    std::ofstream(info_path) << jayson::serialize(info).serialize();
  }

  switch (ctx.config().meta.type) {
  case PackageType::Executable:
    install_executable(ctx.config(), package_ident, install_directory);

    // NOTE: maybe the end user doesn't want to immediately select the
    // version? also, intended to be that one could run select by itself and
    // choose the version of a package to put onto the path.
    select_executable(pkg.db(), package_ident);
    break;

  case PackageType::StaticLibrary:
    install_library(ctx.config(), package_ident, install_directory);
    break;

  case PackageType::SharedLibrary:
    throw std::runtime_error(
      "hewg does not support installing shared libraries");
    break;

  case PackageType::Headers:
    install_headers(ctx.config(), package_ident, install_directory);
    break;
  }

  save_package_db(pkg.db());
}
