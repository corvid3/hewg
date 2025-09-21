#include <algorithm>
#include <compare>
#include <crow.datalogpp/datalogpp.hh>
#include <crow.jayson/jayson.hh>
#include <crow.scl/scl.hh>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "analysis.hh"
#include "common.hh"
#include "confs.hh"
#include "packages.hh"
#include "paths.hh"
#include "semver.hh"
#include "target.hh"

/*
  .hewg/packages
    contains all installed packages,
    by org-name. then each named directory
    contains the unique package instances,
    version-target

  .hewg/packages

*/

std::strong_ordering
PackageIdentifier::operator<=>(PackageIdentifier const& rhs) const
{
  if (this->m_org != rhs.m_org)
    return compare_ascii(this->m_org, rhs.m_org);

  if (this->m_name != rhs.m_name)
    return compare_ascii(this->m_name, rhs.m_name);

  if (this->m_version != rhs.m_version)
    return this->m_version <=> rhs.m_version;

  if (this->m_target != rhs.m_target)
    return compare_ascii(this->m_target.to_string(), rhs.m_target.to_string());

  return std::strong_ordering::equal;
}

std::partial_ordering
DependencyIdentifier::operator<=>(DependencyIdentifier const& rhs) const
{
  auto const s = this->m_packageIdentifier <=> rhs.m_packageIdentifier;

  if (s != std::partial_ordering::equivalent)
    return s;

  if (this->m_sort == rhs.m_sort)
    return std::partial_ordering::equivalent;

  return std::partial_ordering::unordered;
}

std::string_view
sort_to_string(DependencyIdentifier::Sort const s)
{
  switch (s) {
    case DependencyIdentifier::Sort::Exact:
      return "=";

    case DependencyIdentifier::Sort::ThisOrBetter:
      return ">=";
  }

  std::unreachable();
}

std::optional<DependencyIdentifier::Sort>
sort_from_string(std::string_view in)
{
  if (in == "=")
    return DependencyIdentifier::Sort::Exact;

  else if (in == ">=")
    return DependencyIdentifier::Sort::ThisOrBetter;

  else
    return std::nullopt;
}

std::optional<PackageIdentifier>
parse_package_identifier(std::string_view what)
{
  auto const first_dot = what.find('.');
  auto const org = what.substr(0, first_dot);

  if (org.empty() or not std::regex_match(org.begin(), org.end(), regexes::org))
    return std::nullopt;

  auto const first_dash = what.find('-');
  auto const name = what.substr(first_dot + 1, first_dash - first_dot - 1);

  if (name.empty() or
      not std::regex_match(name.begin(), name.end(), regexes::name))
    return std::nullopt;

  auto const first_colon = what.find(':');
  auto const semver = what.substr(first_dash + 1, first_colon - first_dash - 1);

  if (semver.empty() or
      not std::regex_match(semver.begin(), semver.end(), regexes::semver))
    return std::nullopt;

  auto const target = what.substr(first_colon + 1);
  if (target.empty() or
      not std::regex_match(target.begin(), target.end(), regexes::target))
    return std::nullopt;

  auto const semver_parsed = parse_semver(semver);
  if (not semver_parsed)
    return std::nullopt;

  return PackageIdentifier(org, name, *semver_parsed, target);
}

static std::optional<PackageIdentifier>
parse_package_identifier_optional_target(std::string_view const what)
{
  auto const first_dot = what.find('.');
  auto const org = what.substr(0, first_dot);

  if (org.empty() or not std::regex_match(org.begin(), org.end(), regexes::org))
    return std::nullopt;

  auto const first_dash = what.find('-');
  auto const name = what.substr(first_dot + 1, first_dash - first_dot - 1);

  if (name.empty() or
      not std::regex_match(name.begin(), name.end(), regexes::name))
    return std::nullopt;

  auto const first_colon = what.find(':');
  auto const semver = what.substr(first_dash + 1, first_colon - first_dash - 1);

  if (semver.empty() or
      not std::regex_match(semver.begin(), semver.end(), regexes::semver))
    return std::nullopt;

  auto const semver_parsed = parse_semver(semver);
  if (not semver_parsed)
    return std::nullopt;

  if (first_colon == what.npos) {
    return PackageIdentifier(
      org, name, *semver_parsed, TargetTriplet(THIS_TARGET));
  } else {
    auto const target = what.substr(first_colon + 1);
    if (target.empty() or
        not std::regex_match(target.begin(), target.end(), regexes::target))
      return std::nullopt;

    return PackageIdentifier(org, name, *semver_parsed, target);
  }
}

std::optional<DependencyIdentifier>
parse_dependency_identifier(std::string_view const what)
{
  if (what.starts_with("="sv)) {
    auto const ident = parse_package_identifier_optional_target(what.substr(1));

    if (not ident)
      return std::nullopt;

    return DependencyIdentifier(DependencyIdentifier::Sort::Exact, *ident);
  } else if (what.starts_with(">="sv)) {
    auto const ident = parse_package_identifier_optional_target(what.substr(2));

    if (not ident)
      return std::nullopt;

    return DependencyIdentifier(DependencyIdentifier::Sort::ThisOrBetter,
                                *ident);
  } else
    return std::nullopt;
}

std::optional<PackageIdentifier>
select_package_from_dependency_identifier(PackageCacheDB const& db,
                                          DependencyIdentifier dep)
{
  std::vector<PackageIdentifier> candidates;

  if (dep.packageIdentifier().version().major() == 0 and
      dep.sort() == DependencyIdentifier::Sort::ThisOrBetter)
    throw std::runtime_error("greater dependency declared on major version 0");

  // select all packages that are of the same major version of the package,
  // we do not care in any case whatsoever if the major version mismatches
  std::ranges::copy_if(db,
                       std::inserter(candidates, candidates.end()),
                       [&dep](PackageIdentifier const& in) -> bool {
                         auto const& dep_packident = dep.packageIdentifier();

                         return in.org() == dep_packident.org() and
                                in.name() == dep_packident.name() and
                                in.target() == dep_packident.target() and
                                in.version().major() ==
                                  dep_packident.version().major();
                       });

  if (candidates.empty())
    return std::nullopt;

  switch (dep.sort()) {
    case DependencyIdentifier::Sort::ThisOrBetter: {
      std::ranges::sort(
        candidates,
        [](PackageIdentifier const& lhs, PackageIdentifier const& rhs) {
          return lhs.version() > rhs.version();
        });

      if (candidates.front().version() < dep.packageIdentifier().version())
        return std::nullopt;
      else
        return candidates.front();
    } break;

    case DependencyIdentifier::Sort::Exact: {
      auto const found = std::ranges::find(candidates, dep.packageIdentifier());
      if (found == candidates.end())
        return std::nullopt;
      else
        return *found;
    } break;

    default:
      std::unreachable();
  }
}

std::filesystem::path
get_package_directory(PackageIdentifier const& ident)
{
  return hewg_packages_directory / std::format("{}", ident);
}

std::optional<PackageInfo>
get_package_info(PackageIdentifier const ident)
{
  auto const info_conf = get_package_directory(ident) / "manifest.json";

  if (not std::filesystem::exists(info_conf))
    return std::nullopt;

  std::string const&& data = read_file(info_conf);
  return jayson::deserialize<PackageInfo>(jayson::val::parse(std::move(data)));
}

std::filesystem::path
get_packages_include_directory(PackageIdentifier const& ident)
{
  return get_package_directory(ident) / "include";
}

std::filesystem::path
get_packages_static_library_file(PackageIdentifier const& ident,
                                 bool const is_pic)
{
  auto const root = get_package_directory(ident);

  if (is_pic)
    return root / std::format("lib{}-PIE.a", ident.name());
  else
    return root / std::format("lib{}.a", ident.name());
}

PackageCacheDB
open_package_db()
{
  // WARNING: currently, package_db.json is _not_
  // multi-process safe. lockfile should be added later

  if (not std::filesystem::exists(hewg_package_db_path))
    return {};

  auto const&& file = read_file(hewg_package_db_path);
  auto const&& val = jayson::val::parse(file);

  return jayson::deserialize<PackageCacheDB>(val);
}

void
save_package_db(PackageCacheDB const& db)
{
  // WARNING: currently, package_db.json is _not_
  // multi-process safe. lockfile should be added later
  std::vector<PackageIdentifier> ident;

  std::ofstream(hewg_package_db_path) << jayson::serialize(db).serialize(true);
}

std::filesystem::path
create_package_instance(PackageCacheDB& db, PackageIdentifier const package)
{
  auto const found = std::find_if(
    db.begin(), db.end(), [=](auto const v) { return v == package; });

  auto const dir = get_package_directory(package);

  if (found == db.end()) {
    db.insert(package);
    std::filesystem::create_directory(dir);
  }

  return dir;
}
