#pragma once

#include "confs.hh"
#include "semver.hh"
#include "target.hh"

#include <compare>
#include <crow.jayson/jayson.hh>
#include <crow.scl/scl.hh>
#include <filesystem>
#include <optional>
#include <regex>
#include <set>
#include <utility>

namespace regexes
{

inline std::regex const org("^[a-zA-Z0-9_]+$");
inline std::regex const name("^[a-zA-Z0-9_]+$");
inline std::regex const semver(
  R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$)");
inline std::regex const
  target("^([a-zA-Z0-9]+)-([a-zA-Z0-9]+)-([a-zA-Z0-9]+)$");

};

// org.name-version:target
class PackageIdentifier
{
public:
  PackageIdentifier(std::string_view org,
                    std::string_view name,
                    SemVer           version,
                    TargetTriplet    target)
    : m_org(org)
    , m_name(name)
    , m_version(std::move(version))
    , m_target(std::move(target)) {};

  auto
  operator<=>(PackageIdentifier const&) const -> std::strong_ordering;

  auto
  operator<(PackageIdentifier const&) const -> bool
    = default;
  auto
  operator==(PackageIdentifier const&) const -> bool
    = default;

  [[nodiscard]]
  auto
  name() const
  {
    return m_name;
  }

  [[nodiscard]]
  auto
  org() const
  {
    return m_org;
  }
  [[nodiscard]]
  auto
  target() const
  {
    return m_target;
  }
  [[nodiscard]]
  auto
  version() const
  {
    return m_version;
  }

private:
  std::string   m_org;
  std::string   m_name;
  SemVer        m_version;
  TargetTriplet m_target;

public:
  using jayson_fields
    = std::tuple<jayson::obj_field<"org", &PackageIdentifier::m_org>,
                 jayson::obj_field<"name", &PackageIdentifier::m_name>,
                 jayson::obj_field<"version", &PackageIdentifier::m_version>,
                 jayson::obj_field<"target", &PackageIdentifier::m_target>>;

  static bool constexpr jayson_explicitly_constructible = true;
};

std::optional<PackageIdentifier> parse_package_identifier(std::string_view);

class DependencyIdentifier
{
public:
  enum class Sort : uint8_t {
    Exact,
    ThisOrBetter,
  };

  DependencyIdentifier(Sort const sort, PackageIdentifier ident)
    : m_sort(sort)
    , m_packageIdentifier(std::move(ident)) {};

  [[nodiscard]]
  auto
  sort() const
  {
    return m_sort;
  }

  [[nodiscard]]
  auto
  packageIdentifier() const -> PackageIdentifier const&
  {
    return m_packageIdentifier;
  }

  auto
  operator<=>(DependencyIdentifier const&) const -> std::partial_ordering;

private:
  Sort              m_sort;
  PackageIdentifier m_packageIdentifier;

  struct SortDescriptor;

public:
  using jayson_fields = std::tuple<
    jayson::enum_field<"sort", &DependencyIdentifier::m_sort, SortDescriptor>,
    jayson::obj_field<"identifier",
                      &DependencyIdentifier::m_packageIdentifier>>;
  static bool constexpr jayson_explicitly_constructible = true;
};

auto sort_to_string(DependencyIdentifier::Sort) -> std::string_view;
auto sort_from_string(std::string_view)
  -> std::optional<DependencyIdentifier::Sort>;

struct DependencyIdentifier::SortDescriptor {
  static auto
  deserialize(std::string_view in) -> std::optional<Sort>
  {
    return sort_from_string(in);
  }

  static auto
  serialize(Sort const in) -> std::string
  {
    return std::string(sort_to_string(in));
  }
};

auto parse_dependency_identifier(std::string_view)
  -> std::optional<DependencyIdentifier>;

struct PackageInfo {
  PackageIdentifier this_identifier;
  PackageType       type;

  std::set<DependencyIdentifier> internal_dependencies;
  std::set<DependencyIdentifier> external_dependencies;

  static bool constexpr jayson_explicitly_constructible = true;

  using jayson_fields
    = std::tuple<jayson::obj_field<"identifier", &PackageInfo::this_identifier>,
                 jayson::enum_field<"type",
                                    &PackageInfo::type,
                                    ProjectTypeEnumDescriptorJayson>,
                 jayson::obj_field<"internal_dependencies",
                                   &PackageInfo::internal_dependencies>,
                 jayson::obj_field<"external_dependencies",
                                   &PackageInfo::external_dependencies>>;
};

using PackageCacheDB = std::set<PackageIdentifier>;

// attempts to select a semver compatable package identifier
// from a dependency identifier and the installed pacakges
// on the system
// if no suitable package identifier can be selected,
// returns nullopt
auto
select_package_from_dependency_identifier(PackageCacheDB const&,
                                          DependencyIdentifier)
  -> std::optional<PackageIdentifier>;
auto
get_package_directory(PackageIdentifier const&) -> std::filesystem::path;

auto
get_package_info(PackageIdentifier const& dep) -> std::optional<PackageInfo>;

auto
get_packages_include_directory(PackageIdentifier const&)
  -> std::filesystem::path;

auto
get_packages_static_library_file(PackageIdentifier const&, bool is_PIC)
  -> std::filesystem::path;

auto
open_package_db() -> PackageCacheDB;

void
save_package_db(PackageCacheDB const&);

// creates a directory for a specific instance of a package
// in the filesystem, and then registers it into the package DB
auto
create_package_instance(PackageCacheDB& db, PackageIdentifier)
  -> std::filesystem::path;

template<>
struct std::formatter<PackageIdentifier> : std::formatter<std::string> {
  auto
  format(PackageIdentifier const& ver, format_context& ctx) const
  {
    return formatter<string>::format(
      std::format(
        "{}.{}-{}:{}", ver.org(), ver.name(), ver.version(), ver.target()),
      ctx);
  }
};
