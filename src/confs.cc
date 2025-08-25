#include <filesystem>
#include <jayson.hh>
#include <optional>
#include <scl/scl.hh>

#include "analysis.hh"
#include "cmdline.hh"
#include "common.hh"
#include "confs.hh"
#include "paths.hh"
#include "semver.hh"

std::string_view
project_type_to_string(PackageType const t)
{
  static const std::map<PackageType, std::string> mapping = {
    { PackageType::Executable, "executable" },
    { PackageType::StaticLibrary, "library" },
    { PackageType::SharedLibrary, "dynlib" },
    { PackageType::Headers, "headers" },
  };

  return mapping.find(t)->second;
}

std::optional<PackageType>
project_type_from_string(std::string_view s)
{
  static const std::map<std::string, PackageType, std::less<>> mapping = {
    { "executable", PackageType::Executable },
    { "library", PackageType::StaticLibrary },
    { "dynlib", PackageType::SharedLibrary },
    { "headers", PackageType::Headers },
  };

  auto const find = mapping.find(s);
  if (find == mapping.end())
    return std::nullopt;
  return find->second;
}

ConfigurationFile
get_config_file(ToplevelOptions const& options, std::filesystem::path path)
{
  std::string config_filedata = read_file(path);
  scl::file file(config_filedata);

  ConfigurationFile conf;
  scl::deserialize(conf, file);

  SemVer const this_hewg_semver = this_hewg_version;
  auto const requested_semver = parse_semver(conf.meta.hewg_version);

  if (not requested_semver)
    throw std::runtime_error("invalid requested hewg version");

  if (*requested_semver > this_hewg_semver) {
    if (not options.force)
      throw std::runtime_error(
        std::format("hewg project requests version {}, but we have {}",
                    *requested_semver,
                    this_hewg_semver));
    else
      threadsafe_print(
        "forcing execution of build system on mismatched hewg version!");
  }

  auto const& name = conf.project.name;
  if (not check_valid_project_identifier(name))
    throw std::runtime_error(
      std::format("project name <{}> is invalid; it must be alphanumeric "
                  "including underscores",
                  name));

  // now we do some jank where we append vectors
  // depending on the build profile
  return conf;
}

ToolProfile
get_default_tool_profile()
{
#ifdef __linux__
  return ToolProfile{ "linux-gcc" };
#else
#error building only allowed on linux
#endif
};

TargetFile
get_target_file(TargetTriplet const target)
{
  auto const static target_dir = user_hewg_directory / "targets";
  auto const filepath = target_dir / target.to_string();

  if (not std::filesystem::exists(filepath))
    throw std::runtime_error(
      std::format("requested target triplet <{}> tool file doesn't exist",
                  target.to_string()));

  TargetFile into(target);
  auto filedata = read_file(filepath);
  scl::file file(filedata);
  scl::deserialize(into, file, "tools");

  into.triplet = target;

  return into;
}

jayson::val
config_to_project_manifest(ConfigurationFile const&)
{
  throw std::runtime_error("config to project manifest unimplemented");
}
