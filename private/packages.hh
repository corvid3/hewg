#pragma once

#include <filesystem>
#include <list>
#include <memory>
#include <optional>
#include <scl.hh>

#include "confs.hh"

struct PackageIdentifier
{
  PackageIdentifier(std::string_view name,
                    version_triplet version,
                    bool const exact_version)
    : name(std::string(name))
    , version(version)
    , exact_version(exact_version) {};

  explicit PackageIdentifier(Dependency const d)
    : name(d.name)
    , version(d.version)
    , exact_version(d.exact) {};

  bool operator==(PackageIdentifier const& rhs) const
  {
    return name == rhs.name and version == version;
  }

  std::string name;
  version_triplet version;
  bool exact_version;
};

// a packages stripped down version
// of a projects hewg.scl
struct PackageInfo
{
  MetaConf meta;
  ProjectConf project;
  std::vector<Dependency> internal_deps;
  std::vector<Dependency> external_deps;

  using scl_fields = std::tuple<>;
  using scl_recurse =
    std::tuple<scl::field<&PackageInfo::meta, "hewg">,
               scl::field<&PackageInfo::project, "project">,
               scl::field<&PackageInfo::internal_deps, "internal">,
               scl::field<&PackageInfo::external_deps, "external">>;
};

struct Package
{
  std::string name;
  version_triplet version;

  std::list<std::shared_ptr<Package>> internal_dependencies;
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

// version is exact
std::shared_ptr<Package>
construct_dependency_graph(std::string_view package_name,
                           version_triplet version);

PackageInfo
get_package_info(std::string_view name, version_triplet requested_version);

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
