#include "analysis.hh"
#include "confs.hh"
#include "packages.hh"
#include "paths.hh"
#include <algorithm>
#include <format>
#include <scl.hh>
#include <stdexcept>

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

PackageInfo
get_package_info(std::string_view name, version_triplet requested_version)
{
  // auto const version_manifest = open_version_manifest(name);
  auto const versioned_package_dir =
    hewg_packages_directory / name /
    version_triplet_to_string(requested_version);
  auto const info_conf = versioned_package_dir / "info.scl";

  scl::file package_info_scl;
  PackageInfo package_info;
  scl::deserialize(package_info, package_info_scl);

  return package_info;
}

std::optional<std::filesystem::path>
try_get_compatable_package(std::string_view name,
                           version_triplet requested_version)
{
  auto const version_manifest = open_version_manifest(name);
  auto const best_version =
    select_best_compatable_semver(version_manifest.versions, requested_version);

  if (not best_version)
    throw std::runtime_error(
      std::format("unable to get a compatable version for package <{}>, "
                  "requested version <{}>",
                  name,
                  version_triplet_to_string(requested_version)));

  return hewg_packages_directory / name /
         version_triplet_to_string(*best_version);
}

VersionManifest
open_version_manifest(std::string_view package_name)
{
  auto const dir = hewg_packages_directory / package_name;
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

void
write_version_manifest(std::string_view package_name,
                       VersionManifest const& manifest)
{
  auto const dir = hewg_packages_directory / package_name;
  auto const version_manifest_file_path = dir / "manifest.json";

  jayson::val v = jayson::serialize(manifest);
  std::ofstream(version_manifest_file_path) << v.serialize();
}

std::filesystem::path
add_version_to_package(std::string_view package_name,
                       version_triplet const version_triplet)
{
  auto const version_path = hewg_packages_directory / package_name /
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
