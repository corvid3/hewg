#pragma once

#include <compare>
#include <filesystem>
#include <jayson.hh>
#include <list>
#include <memory>
#include <optional>
#include <scl.hh>
#include <set>

#include "common.hh"
#include "semver.hh"
#include "target.hh"

// versions must be an exact match
class PackageIdentifier
{
public:
  // TODO: parse
  // PackageIdentifier(std::string_view text);

  PackageIdentifier(std::string_view name,
                    std::string_view org,
                    TargetTriplet target,
                    SemVer version)
    : m_name(name)
    , m_org(org)
    , m_target(target)
    , m_version(version) {};

  std::partial_ordering operator<=>(PackageIdentifier const&) const;
  bool operator<(PackageIdentifier const&) const = default;
  bool operator==(PackageIdentifier const&) const = default;

  auto name() const { return m_name; }
  auto org() const { return m_org; }
  auto target() const { return m_target; }
  auto version() const { return m_version; }

private:
  std::string m_name;
  std::string m_org;
  TargetTriplet m_target;
  SemVer m_version;

public:
  using jayson_fields =
    std::tuple<jayson::obj_field<"name", &PackageIdentifier::m_name>,
               jayson::obj_field<"org", &PackageIdentifier::m_org>,
               jayson::obj_field<"target", &PackageIdentifier::m_target>,
               jayson::obj_field<"version", &PackageIdentifier::m_version>>;

  static bool constexpr jayson_explicitly_constructible = true;
};

std::string
format_package_identifier();

class DependencyIdentifier
{
public:
  enum class Sort
  {
    Exact,
    ThisOrBetter,
  };

  DependencyIdentifier(std::string_view name,
                       std::string_view org,
                       Sort sort,
                       SemVer semver,
                       TargetTriplet target)
    : m_name(name)
    , m_org(org)
    , m_sort(sort)
    , m_semver(semver)
    , m_target(target) {};

  auto name() const { return m_name; }
  auto org() const { return m_org; }
  auto sort() const { return m_sort; }
  auto semver() const { return m_semver; }
  auto target() const { return m_target; }

private:
  std::string m_name;
  std::string m_org;
  Sort m_sort;
  SemVer m_semver;
  TargetTriplet m_target;
};

// a packages stripped down version
// of a projects hewg.scl
struct PackageInfo
{
  MetaConf meta;
  ProjectConf project;
  DependenciesConf depends;

  using scl_fields = std::tuple<>;
  using scl_recurse = std::tuple<scl::field<&PackageInfo::meta, "hewg">,
                                 scl::field<&PackageInfo::project, "project">,
                                 scl::field<&PackageInfo::depends, "depends">>;
};

using PackageCacheDB = std::set<PackageIdentifier>;

struct Package
{
  std::string name;
  version_triplet version;

  std::list<std::shared_ptr<Package>> internal_dependencies;
};

// struct ResolvedDependencies
// {
//   std::vector<PackageInfo> packages_to_link;
//   std::vector<PackageInfo> packages_to_include;
// };

// std::vector<ResolvedDependencies>
// resolve_deps(ConfigurationFile const& config);

// attempts to select a semver compatable package identifier
// from a dependency identifier and the installed pacakges
// on the system
// if no suitable package identifier can be selected,
// returns nullopt
std::optional<PackageIdentifier> select_package_from_dependency_identifier(
  DependencyIdentifier);

void
build_dependency_tree(ConfigurationFile const& config);

std::filesystem::path
get_package_directory(PackageIdentifier const&);

// version is exact
std::shared_ptr<Package>
construct_dependency_graph(std::string_view package_name,
                           version_triplet version);

PackageInfo
get_package_info(PackageIdentifier dep);

PackageCacheDB
open_package_db();

void
save_package_db(PackageCacheDB const&);

// creates a directory for a specific instance of a package
// in the filesystem, and then registers it into the package DB
std::filesystem::path
create_package_instance(PackageCacheDB& db, PackageIdentifier);

template<>
struct std::formatter<PackageIdentifier> : std::formatter<std::string>
{
  auto format(PackageIdentifier const& ver, format_context& ctx) const
  {
    return formatter<string>::format(
      std::format(
        "{}.{}-{}:{}", ver.org(), ver.name(), ver.version(), ver.target()),
      ctx);
  }
};
