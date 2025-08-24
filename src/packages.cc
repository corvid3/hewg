#include <algorithm>
#include <compare>
#include <datalogpp.hh>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <jayson.hh>
#include <optional>
#include <scl.hh>
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

/*

  dependency graph must be a noncyclic
  directional graph.

  packages are differentiated by both name and version,
  so multiple packages of the same name may show up in
  a graph, but differ by version.



  random thoughts:
    internal dependencies must only be publicly enumerated
    for static libraries, as they get linked later

  a cyclic dependency is based entirely on name, not versioning

*/

// std::shared_ptr<Package>
// construct_dependency_graph(std::string_view package_name,
//                            version_triplet version)
// {
//   std::map<PackageIdentifier, std::shared_ptr<Package>> package_cache;

//   auto const add_to_cache = [&](PackageIdentifier const what) {
//     std::shared_ptr<Package> pck(new Package());
//     package_cache.insert_or_assign(what, pck);
//     return pck;
//   };

//   auto const insert_recurse = [&](PackageIdentifier const ident) {
//     auto this_package = add_to_cache(ident);

//     VersionManifest const manifest = open_version_manifest(package_name);
//     if (not std::ranges::contains(manifest.versions, version))
//       throw std::runtime_error(
//         "version not found in package while constructing dependency");

//     auto const versioned_package_dir = hewg_packages_directory / package_name
//     /
//                                        version_triplet_to_string(version);

//     auto const info = get_package_info(package_name, version);
//     for (auto const& dep : info.internal_deps) {
//       if (not package_cache.contains(PackageIdentifier(dep)))
//       // this_package->internal_dependencies.push_back(insert_recurse(dep));
//     }
//   };

//   // insert_recurse
// }

std::optional<PackageIdentifier>
select_package_from_dependency_identifier(PackageCacheDB const& db,
                                          DependencyIdentifier dep)
{
  std::vector<PackageIdentifier> candidates;

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

using namespace datalogpp;

struct DepTreeCtx
{
  /*
    Package(ORG, NAME, VERSION, PACKAGE_TYPE, EXTERNAL_INTERNAL).
    Edge(NAME_PARENT, VERSION_PARENT, NAME_CHILD, VERSION_CHILD).
  */

  Interpreter interpreter;
  Predicate &package, &dependency, &dependency_path, &static_to_static_deps,
    &exists_extern, &dependency_cycle, &extern_chain;

  DepTreeCtx()
    : interpreter()
    , package(interpreter.predicate(
        "Package",
        std::array{ "ORG", "NAME", "VERSION", "TARGET", "TYPE" }.size()))
    , dependency(interpreter.predicate("Dependency",
                                       std::array{ "P_ORG",
                                                   "P_NAME",
                                                   "P_VERSION",
                                                   "P_TARGET",
                                                   "C_ORG",
                                                   "C_NAME",
                                                   "C_VERSION",
                                                   "C_TARGET",
                                                   "DEPTYPE",
                                                   "EXT" }
                                         .size()))
    , dependency_path(interpreter.predicate("DependencyPath", 8))
    , static_to_static_deps(
        interpreter.predicate("StaticToStaticDependencies", 0))
    , exists_extern(interpreter.predicate("ExistsExternalDependency", 0))
    , dependency_cycle(interpreter.predicate("DependencyCycle", 0))
    , extern_chain(interpreter.predicate("ExternalDependencyChain", 8))
  {
    using namespace datalogpp;

    static_to_static_deps() =
      ("Package"_p("ORG"_V, "NAME"_V, "VERSION"_V, "TARGET"_V, "static"),
       "Dependency"_p("ORG"_V,
                      "NAME"_V,
                      "VERSION"_V,
                      "TARGET"_V,
                      "C_ORG"_V,
                      "C_NAME"_V,
                      "C_VERSION"_V,
                      "C_TARGET"_V,
                      "_"_V,
                      "_"_V),
       "Package"_p(
         "C_ORG"_V, "C_NAME"_V, "C_VERSION"_V, "C_TARGET"_V, "static"));

    exists_extern() = ("Dependency"_p("_"_V,
                                      "_"_V,
                                      "_"_V,
                                      "_"_V,
                                      "_"_V,
                                      "_"_V,
                                      "_"_V,
                                      "_"_V,
                                      "_"_V,
                                      "external"));

    dependency_path("P_ORG"_V,
                    "P_NAME"_V,
                    "P_VERSION"_V,
                    "P_TARGET"_V,
                    "C_ORG"_V,
                    "C_NAME"_V,
                    "C_VERSION"_V,
                    "C_TARGET"_V) = ("Dependency"_p("P_ORG"_V,
                                                    "P_NAME"_V,
                                                    "P_VERSION"_V,
                                                    "P_TARGET"_V,
                                                    "C_ORG"_V,
                                                    "C_NAME"_V,
                                                    "C_VERSION"_V,
                                                    "C_TARGET"_V,
                                                    "_"_V,
                                                    "_"_V));

    dependency_path("P_ORG"_V,
                    "P_NAME"_V,
                    "P_VERSION"_V,
                    "P_TARGET"_V,
                    "C_ORG"_V,
                    "C_NAME"_V,
                    "C_VERSION"_V,
                    "C_TARGET"_V) = ("Dependency"_p("P_ORG"_V,
                                                    "P_NAME"_V,
                                                    "P_VERSION"_V,
                                                    "P_TARGET"_V,
                                                    "I_ORG"_V,
                                                    "I_NAME"_V,
                                                    "I_VERSION"_V,
                                                    "I_TARGET"_V,
                                                    "_"_V,
                                                    "_"_V),
                                     "Dependency"_p("I_ORG"_V,
                                                    "I_NAME"_V,
                                                    "I_VERSION"_V,
                                                    "I_TARGET"_V,
                                                    "C_ORG"_V,
                                                    "C_NAME"_V,
                                                    "C_VERSION"_V,
                                                    "C_TARGET"_V,
                                                    "_"_V,
                                                    "_"_V));

    dependency_cycle() = "DependencyPath"_p(
      "ORG"_V, "NAME"_V, "_"_V, "_"_V, "ORG"_V, "NAME"_V, "_"_V, "_"_V);

    extern_chain("P_ORG"_V,
                 "P_NAME"_V,
                 "P_VERSION"_V,
                 "P_TARGET"_V,
                 "C_ORG"_V,
                 "C_NAME"_V,
                 "C_VERSION"_V,
                 "C_TARGET"_V) = ("Dependency"_p("P_ORG"_V,
                                                 "P_NAME"_V,
                                                 "P_VERSION"_V,
                                                 "P_TARGET"_V,
                                                 "C_ORG"_V,
                                                 "C_NAME"_V,
                                                 "C_VERSION"_V,
                                                 "C_TARGET"_V,
                                                 "_"_V,
                                                 "external"));

    extern_chain("P_ORG"_V,
                 "P_NAME"_V,
                 "P_VERSION"_V,
                 "P_TARGET"_V,
                 "C_ORG"_V,
                 "C_NAME"_V,
                 "C_VERSION"_V,
                 "C_TARGET"_V) = ("Dependency"_p("P_ORG"_V,
                                                 "P_NAME"_V,
                                                 "P_VERSION"_V,
                                                 "P_TARGET"_V,
                                                 "I_ORG"_V,
                                                 "I_NAME"_V,
                                                 "I_VERSION"_V,
                                                 "I_TARGET"_V,
                                                 "_"_V,
                                                 "external"_V),
                                  "Dependency"_p("I_ORG"_V,
                                                 "I_NAME"_V,
                                                 "I_VERSION"_V,
                                                 "I_TARGET"_V,
                                                 "C_ORG"_V,
                                                 "C_NAME"_V,
                                                 "C_VERSION"_V,
                                                 "C_TARGET"_V,
                                                 "_"_V,
                                                 "external"_V));

    // package("crow", "scl", "0.3.0", "x86-linux-gnu", "static") = {};
    // package("crow", "lexible", "0.4.0", "x86-linux-gnu", "static") = {};
    // package("crow", "bar", "0.2.0", "x86-linux-gnu", "static") = {};

    // dependency("crow",
    //            "scl",
    //            "0.3.0",
    //            "x86-linux-gnu",
    //            "crow",
    //            "lexible",
    //            "0.4.0",
    //            "x86-linux-gnu",
    //            ">",
    //            "external") = {};

    // dependency("crow",
    //            "lexible",
    //            "0.4.0",
    //            "x86-linux-gnu",
    //            "crow",
    //            "bar",
    //            "0.2.0",
    //            "x86-linux-gnu",
    //            ">",
    //            "external") = {};

    interpreter.infer();
  };

  void add_package(std::string_view org,
                   std::string_view name,
                   std::string_view version,
                   std::string_view target,
                   std::string_view package_type)
  {
    package(std::string(org),
            std::string(name),
            std::string(version),
            std::string(target),
            std::string(package_type)) = {};
  }

  void add_package(PackageIdentifier const& ident,
                   std::string_view package_type)
  {
    add_package(ident.org(),
                ident.name(),
                std::format("{}", ident.version()),
                std::format("{}", ident.target()),
                package_type);
  }

  void add_dependency(PackageIdentifier const& parent,
                      DependencyIdentifier const& child,
                      bool const is_external)
  {
    dependency(std::string(parent.org()),
               std::string(parent.name()),
               std::format("{}", parent.version()),
               std::format("{}", parent.target()),
               std::string(child.packageIdentifier().org()),
               std::string(child.packageIdentifier().name()),
               std::format("{}", child.packageIdentifier().version()),
               std::format("{}", child.packageIdentifier().target()),
               std::string(sort_to_string(child.sort())),
               std::string(is_external ? "external" : "internal")) = {};
  }
};

static void
add_to_dependency_tree(DepTreeCtx& ctx,
                       PackageCacheDB& db,
                       std::set<PackageIdentifier>& visited_packages,
                       PackageIdentifier const& identifier)
{
  std::println(
    "nungus {} {}", identifier, visited_packages.contains(identifier));
  if (visited_packages.contains(identifier))
    return;
  std::println("nungus");

  visited_packages.insert(identifier);

  if (not db.contains(identifier)) {
    std::println("what");
    throw std::runtime_error(
      std::format("package {} does not exist", identifier));
  }

  auto const this_package_info = get_package_info(identifier);
  if (not this_package_info)
    throw std::runtime_error(
      std::format("package {} does not contain a manifest.json?", identifier));

  ctx.add_package(identifier, this_package_info->package_type);

  for (auto const& internal : this_package_info->internal_dependencies) {
    auto const internal_identifier = internal.packageIdentifier();
    add_to_dependency_tree(ctx, db, visited_packages, internal_identifier);
    ctx.add_dependency(identifier, internal, false);
  }

  for (auto const& external : this_package_info->external_dependencies) {
    auto const external_identifier = external.packageIdentifier();
    add_to_dependency_tree(ctx, db, visited_packages, external_identifier);
    ctx.add_dependency(identifier, external, true);
  }
}

void
build_dependency_tree(ConfigurationFile const& config,
                      PackageCacheDB& db,
                      TargetTriplet const& this_target)
{
  DepTreeCtx ctx;
  std::set<PackageIdentifier> visited_packages;
  auto const this_package_identifier =
    get_this_package_ident(config, this_target);
  visited_packages.insert(this_package_identifier);

  for (auto const& x : db)
    std::println("{}", x.name());

  // add this package
  ctx.add_package(config.project.org,
                  config.project.name,
                  config.project.version,
                  project_type_to_string(config.meta.type),
                  this_target.to_string());

  for (auto const& internal : config.depends.internal) {
    auto const dependency_identifier = parse_dependency_identifier(internal);

    if (not dependency_identifier)
      throw std::runtime_error(std::format(
        "invalid dependency identifier in internal dependencies, '{}'",
        internal));

    auto const depend_package_ident =
      dependency_identifier->packageIdentifier();

    add_to_dependency_tree(ctx, db, visited_packages, depend_package_ident);
    ctx.add_dependency(this_package_identifier, *dependency_identifier, false);
  }

  for (auto const& external : config.depends.external) {
    auto const dependency_identifier = parse_dependency_identifier(external);

    if (not dependency_identifier)
      throw std::runtime_error(std::format(
        "invalid dependency identifier in external dependencies, '{}'",
        external));

    auto const depend_package_ident =
      dependency_identifier->packageIdentifier();

    add_to_dependency_tree(ctx, db, visited_packages, depend_package_ident);
    ctx.add_dependency(this_package_identifier, *dependency_identifier, true);
  }

  // run the datalog engine!
  // for large trees, this is where all of the
  // perf goes
  ctx.interpreter.infer();

  std::cout << ctx.interpreter.dump_facts();

  // auto& package = dl.predicate("package", 4);
  // package("foo", "3.14.0", "static", "external") = {};
  // auto& external = dl.predicate("external", 1);
  // external("X"_V) = ("package"_p("X"_V, "Y"_V, "Z"_V, "external"));
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
