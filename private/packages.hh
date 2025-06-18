#pragma once

#include <filesystem>
#include <optional>

#include "confs.hh"

struct Dependency
{
  ProjectConf project_info;

  std::filesystem::path dependency_root;
};

struct VersionManifest
{
  std::string name;
  std::vector<std::tuple<int, int, int>> versions;

  using jayson_fields =
    std::tuple<jayson::obj_field<"name", &VersionManifest::name>,
               jayson::obj_field<"versions", &VersionManifest::versions>>;
};

// attempts to open the directory
// where the version of a specific
// package lives
std::optional<std::filesystem::path>
try_get_compatable_package(std::string_view name,
                           version_triplet requested_version);

std::vector<Dependency>
enumerate_all_dependencies(ConfigurationFile const& config);

VersionManifest
open_version_manifest(std::string_view package_name);

void
write_version_manifest(std::string_view package_name,
                       VersionManifest const& manifest);

// returns the directory where the version
// of the hewg project is stored
std::filesystem::path
add_version_to_package(std::string_view package_name,
                       version_triplet const version_triplet);
