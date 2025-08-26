#pragma once

#include <compare>
#include <crow.jayson/jayson.hh>
#include <crow.scl/scl.hh>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>

#include "confs.hh"
#include "semver.hh"
#include "target.hh"

namespace regexes {

std::regex inline const org("^[a-zA-Z]+$");
std::regex inline const name("^[a-zA-Z0-9]+$");
std::regex inline const semver(
  R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$)");
std::regex inline const target(
  "^([a-zA-Z0-9]+)-([a-zA-Z0-9]+)-([a-zA-Z0-9]+)$");

};

// org.name-version:target
class PackageIdentifier
{
public:
  PackageIdentifier(std::string_view org,
                    std::string_view name,
                    SemVer version,
                    TargetTriplet target)
    : m_org(org)
    , m_name(name)
    , m_version(version)
    , m_target(target) {};

  std::strong_ordering operator<=>(PackageIdentifier const&) const;
  bool operator<(PackageIdentifier const&) const = default;
  bool operator==(PackageIdentifier const&) const = default;

  auto name() const { return m_name; }
  auto org() const { return m_org; }
  auto target() const { return m_target; }
  auto version() const { return m_version; }

private:
  std::string m_org;
  std::string m_name;
  SemVer m_version;
  TargetTriplet m_target;

public:
  using jayson_fields =
    std::tuple<jayson::obj_field<"org", &PackageIdentifier::m_org>,
               jayson::obj_field<"name", &PackageIdentifier::m_name>,
               jayson::obj_field<"version", &PackageIdentifier::m_version>,
               jayson::obj_field<"target", &PackageIdentifier::m_target>>;

  static bool constexpr jayson_explicitly_constructible = true;
};

std::optional<PackageIdentifier> parse_package_identifier(std::string_view);

class DependencyIdentifier
{
public:
  enum class Sort
  {
    Exact,
    ThisOrBetter,
  };

  DependencyIdentifier(Sort const sort, PackageIdentifier const ident)
    : m_sort(sort)
    , m_packageIdentifier(ident) {};

  auto sort() const { return m_sort; }
  auto const& packageIdentifier() const { return m_packageIdentifier; }

  std::partial_ordering operator<=>(DependencyIdentifier const&) const;

private:
  Sort m_sort;
  PackageIdentifier m_packageIdentifier;

  struct SortDescriptor;

public:
  using jayson_fields = std::tuple<
    jayson::enum_field<"sort", &DependencyIdentifier::m_sort, SortDescriptor>,
    jayson::obj_field<"identifier",
                      &DependencyIdentifier::m_packageIdentifier>>;
  static bool constexpr jayson_explicitly_constructible = true;
};

std::string_view sort_to_string(DependencyIdentifier::Sort);
std::optional<DependencyIdentifier::Sort> sort_from_string(std::string_view);

struct DependencyIdentifier::SortDescriptor
{
  std::optional<Sort> static deserialize(std::string_view in)
  {
    return sort_from_string(in);
  }

  std::string static serialize(Sort const in)
  {
    return std::string(sort_to_string(in));
  }
};

std::optional<DependencyIdentifier> parse_dependency_identifier(
  std::string_view);

struct PackageInfo
{
  PackageIdentifier this_identifier;
  PackageType type;

  std::set<DependencyIdentifier> internal_dependencies;
  std::set<DependencyIdentifier> external_dependencies;

  using jayson_fields =
    std::tuple<jayson::obj_field<"identifier", &PackageInfo::this_identifier>,
               jayson::enum_field<"type",
                                  &PackageInfo::type,
                                  ProjectTypeEnumDescriptorJayson>,
               jayson::obj_field<"internal_dependencies",
                                 &PackageInfo::internal_dependencies>,
               jayson::obj_field<"external_dependencies",
                                 &PackageInfo::external_dependencies>>;

  static bool constexpr jayson_explicitly_constructible = true;
};

using PackageCacheDB = std::set<PackageIdentifier>;

// attempts to select a semver compatable package identifier
// from a dependency identifier and the installed pacakges
// on the system
// if no suitable package identifier can be selected,
// returns nullopt
std::optional<PackageIdentifier>
select_package_from_dependency_identifier(PackageCacheDB const&,
                                          DependencyIdentifier);
// creates and verifies the dependency tree
// for this hewg project

struct DeptreeCtx;
struct DeptreeDeleter
{
  void operator()(DeptreeCtx*);
};
using Deptree = std::unique_ptr<DeptreeCtx, DeptreeDeleter>;

Deptree
build_dependency_tree(ConfigurationFile const& config,
                      PackageCacheDB const& db,
                      TargetTriplet const& this_target);

std::set<PackageIdentifier>
collect_packages_to_include(ConfigurationFile const& config,
                            PackageCacheDB const& db,
                            TargetTriplet const& this_target,
                            Deptree const&);

std::set<PackageIdentifier>
collect_packages_to_link(ConfigurationFile const& config,
                         PackageCacheDB const& db,
                         TargetTriplet const& this_target,
                         Deptree const&);

std::filesystem::path
get_package_directory(PackageIdentifier const&);

std::optional<PackageInfo>
get_package_info(PackageIdentifier dep);

std::filesystem::path
get_packages_include_directory(PackageIdentifier const&);

std::filesystem::path
get_packages_static_library_file(PackageIdentifier const&, bool is_PIC);

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
