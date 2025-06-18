#include "analysis.hh"
#include "confs.hh"
#include "packages.hh"
#include "paths.hh"
#include <format>
#include <stdexcept>

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

std::vector<Dependency>
enumerate_all_dependencies(ConfigurationFile const& config)
{
  std::set<Dependency> deps;

  for (auto const& dep : config.libs.packages) {
    auto const [depname, req_version] = dep;

    // fora
  }

  return {};
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
