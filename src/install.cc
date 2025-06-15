#include "common.hh"
#include "confs.hh"
#include "install.hh"
#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <jayson.hh>
#include <ostream>
#include <scl.hh>
#include <sstream>
#include <stdexcept>
#include <vector>

/*

  since multiple versions of a binary
  can be installed at once, it's a smart idea
  to have a symlink to the binary in just a /bin
  directory.

  this also means there needs to be a way to select
  and change the currently used version of the binary,
  to swap out the symlink to point to another version

*/

struct VersionManifest
{
  std::string name;
  std::vector<std::tuple<int, int, int>> versions;

  using jayson_fields =
    std::tuple<jayson::obj_field<"name", &VersionManifest::name>,
               jayson::obj_field<"versions", &VersionManifest::versions>>;
};

auto const user_hewg_directory = get_home_directory() / ".hewg";
auto const packages_directory = user_hewg_directory / "packages";
auto const bin_directory = user_hewg_directory / "bin";

// returns the path to the user hewg directory
static std::filesystem::path
ensure_user_hewg_directory()
{
  create_directory_checked(user_hewg_directory);
  create_directory_checked(packages_directory);
  create_directory_checked(bin_directory);

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
  auto const package_dir = packages_directory / package_name;

  std::filesystem::create_directories(package_dir);

  if (not std::filesystem::exists(package_dir / "manifest.json")) {
    std::ofstream(package_dir / "manifest.json")
      << skel_manifest(package_name).serialize(false);
  }
}

static VersionManifest
open_version_manifest(std::string_view package_name)
{
  auto const dir = packages_directory / package_name;
  auto const version_manifest_file_path = dir / "manifest.json";

  if (not std::filesystem::exists(dir))
    throw std::runtime_error("attempting to open a version manifest in a "
                             "package directory that doesn't exist");

  std::stringstream ss;
  ss << std::ifstream(version_manifest_file_path).rdbuf();
  jayson::val v = jayson::val::parse(std::move(ss).str());
  VersionManifest out;
  jayson::deserialize(v, out);
  return out;
}

static void
write_version_manifest(std::string_view package_name,
                       VersionManifest const& manifest)
{
  auto const dir = packages_directory / package_name;
  auto const version_manifest_file_path = dir / "manifest.json";

  jayson::val v = jayson::serialize(manifest);
  std::ofstream(version_manifest_file_path) << v.serialize();
}

// returns the directory where the version
// of this hewg project will be stored
static std::filesystem::path
add_version_to_package(std::string_view package_name,
                       version_triplet const version_triplet)
{
  auto const version_path = packages_directory / package_name /
                            version_triplet_to_string(version_triplet);

  VersionManifest manifest = open_version_manifest(package_name);

  auto const found =
    std::find_if(manifest.versions.begin(),
                 manifest.versions.end(),
                 [=](auto const v) { return v == version_triplet; });

  if (found == manifest.versions.end()) {
    manifest.versions.push_back(version_triplet);
    std::filesystem::create_directory(version_path);
  }

  write_version_manifest(package_name, manifest);

  return version_path;
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
    packages_directory / name / version_triplet_to_string(trip);
  auto const exec_path = package_dir / name;

  std::filesystem::create_symlink(exec_path, bin_directory / name);
}

static void
install_executable(ConfigurationFile const& config,
                   std::string_view profile,
                   std::filesystem::path const& install_dir)
{
  auto const executable_name = config.project.name;
  auto const executable_path =
    std::format("target/{}/{}", profile, executable_name);

  std::filesystem::copy_file(executable_path,
                             install_dir / executable_name,
                             std::filesystem::copy_options::update_existing);

  auto const info_path = install_dir / "info.scl";

  // we want just the meta & project information
  scl::scl_file file;
  scl::serialize(config.meta, file, "hewg");
  scl::serialize(config.project, file, "project");

  std::ofstream(info_path) << file.serialize();

  select_executable(executable_name, config.project.version);
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

  switch (config.meta.type) {
    case ProjectType::Executable:
      install_executable(config, profile, install_directory);
      break;

    case ProjectType::StaticLibrary:
      throw std::runtime_error(
        "hewg does not support installing static libraries");
      break;

    case ProjectType::SharedLibrary:
      throw std::runtime_error(
        "hewg does not support installing shared libraries");
      break;
  }
}

// std::filesystem::path
// get_package_by_name(std::string_view name)
// {
// }
