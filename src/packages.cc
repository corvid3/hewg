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
#include <vector>

#include "common.hh"
#include "packages.hh"
#include "paths.hh"

/*
  .hewg/packages
    contains all installed packages,
    by org-name. then each named directory
    contains the unique package instances,
    version-target

  .hewg/packages

*/

std::partial_ordering
PackageIdentifier::operator<=>(PackageIdentifier const& rhs) const
{
  if (this->m_name == rhs.m_name and this->m_org == rhs.m_org and
      this->m_target == rhs.m_target) {
    if (this->m_version == rhs.m_version)
      return std::partial_ordering::equivalent;
    else
      return this->m_version <=> rhs.m_version;
  }

  return std::partial_ordering::unordered;
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

using namespace datalogpp;

// struct DepTreeCtx
// {
//   /*
//     Package(NAME, VERSION, PACKAGE_TYPE, EXTERNAL_INTERNAL).
//     Edge(NAME_PARENT, VERSION_PARENT, NAME_CHILD, VERSION_CHILD).
//   */

//   DepTreeCtx()
//     : interpreter()
//     , package(interpreter.predicate("package", 4))
//     , edge(interpreter.predicate("edge", 4)) {};

//   Interpreter interpreter;
//   Predicate& package;
//   Predicate& edge;
// };

std::optional<PackageIdentifier>
select_package_from_dependency_identifier(PackageCacheDB const& db,
                                          DependencyIdentifier dep)
{
  std::vector<PackageIdentifier> candidates;
  std::ranges::copy_if(db,
                       std::inserter(candidates, candidates.end()),
                       [&dep](PackageIdentifier const& in) -> bool {
                         return in.org() == dep.org() and
                                in.name() == dep.name() and
                                in.target() == dep.target() and
                                in.version().major() == dep.semver().major();
                       });

  if (candidates.empty())
    return std::nullopt;

  std::ranges::sort(
    candidates, [](PackageIdentifier const& lhs, PackageIdentifier const& rhs) {
      return lhs.version() > rhs.version();
    });

  return candidates.front();
}

// static void
// build_dependency_tree_recurse(DepTreeCtx& interp,
//                               PackageIdentifier self,
//                               bool is_external,
//                               std::vector<PackageIdentifier>& visited)
// {
//   auto const& package_info = get_package_info(self);

//   std::string package_type;

//   switch (package_info.meta.type) {
//     case ProjectType::Executable:
//     case ProjectType::Headers:
//       throw std::runtime_error("unallowable package type in dependency
//       tree");

//     case ProjectType::StaticLibrary:
//       package_type = "static";
//       break;

//     case ProjectType::SharedLibrary:
//       package_type = "dynlib";
//   }

//   interp.package(self.name(),
//                  version_triplet_to_string(self.version()),
//                  package_type,
//                  is_external ? "external" : "internal") = {};

//   visited.push_back(self);

//   for (auto const& ext_dep : package_info.external_deps) {
//     auto const dep = *select_package_from_dep(ext_dep);

//     if (std::ranges::contains(visited, dep))
//       continue;

//     build_dependency_tree_recurse(interp, dep, true, visited);
//     interp.edge(self.name(),
//                 version_triplet_to_string(self.version()),
//                 dep.name(),
//                 version_triplet_to_string(dep.version())) = {};
//   }

//   for (auto const& internal_dep : package_info.internal_deps) {
//     auto const dep = *select_package_from_dep(internal_dep);

//     if (std::ranges::contains(visited, dep))
//       continue;

//     build_dependency_tree_recurse(interp, dep, true, visited);
//     interp.edge(self.name(),
//                 version_triplet_to_string(self.version()),
//                 dep.name(),
//                 version_triplet_to_string(dep.version())) = {};
//   }
// }

// void
// build_dependency_tree(ConfigurationFile const& config)
// {
//   DepTreeCtx ctx;
//   std::vector<PackageIdentifier> visited_packages;

//   for (auto const& external : config.external_deps)
//     build_dependency_tree_recurse(
//       ctx, *select_package_from_dep(external), true, visited_packages);

//   for (auto const& internal : config.internal_deps)
//     build_dependency_tree_recurse(
//       ctx, *select_package_from_dep(internal), true, visited_packages);

//   std::cout << ctx.interpreter.dump_facts();

//   // auto& package = dl.predicate("package", 4);
//   // package("foo", "3.14.0", "static", "external") = {};
//   // auto& external = dl.predicate("external", 1);
//   // external("X"_V) = ("package"_p("X"_V, "Y"_V, "Z"_V, "external"));
// }

std::filesystem::path
get_package_directory(PackageIdentifier const& ident)
{
  return hewg_packages_directory / std::format("{}", ident);
}

PackageInfo
get_package_info(PackageIdentifier const ident)
{
  auto const instance_dir = get_package_directory(ident);
  auto const info_conf = instance_dir / "info.scl";

  scl::file package_info_scl;
  PackageInfo package_info;
  scl::deserialize(package_info, package_info_scl);

  return package_info;
}

PackageCacheDB
open_package_db()
{
  // WARNING: currently, package_db.json is _not_
  // multi-process safe. lockfile should be added later

  if (not std::filesystem::exists(hewg_package_db_path))
    return {};

  return jayson::deserialize<PackageCacheDB>(
    jayson::val(read_file(hewg_package_db_path)));
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
