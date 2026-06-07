#include "analysis.hh"
#include "cmdline.hh"
#include "common.hh"
#include "confs.hh"
#include "paths.hh"
#include "semver.hh"

#include <crow.jayson/jayson.hh>
#include <crow.scl/scl.hh>
#include <filesystem>
#include <format>
#include <optional>
#include <utility>

/* target dependent configuration tables */
struct CXXTargetConf {
  /* added to the main source listing */
  std::vector<std::string> source_files;

  using scl_fields
    = std::tuple<scl::field<&CXXTargetConf::source_files, "sources">>;
};

struct CTargetConf {
  /* added to the main source listing */
  std::vector<std::string> source_files;

  using scl_fields
    = std::tuple<scl::field<&CTargetConf::source_files, "sources">>;
};

auto
project_type_to_string(PackageType const t) -> std::string_view
{
  static std::map<PackageType, std::string> const mapping = {
    {    PackageType::Executable, "executable" },
    { PackageType::StaticLibrary,    "library" },
    { PackageType::SharedLibrary,     "dynlib" },
    {       PackageType::Headers,    "headers" },
  };

  return mapping.find(t)->second;
}

auto
project_type_from_string(std::string_view s) -> std::optional<PackageType>
{
  static std::map<std::string, PackageType, std::less<>> const mapping = {
    { "executable",    PackageType::Executable },
    {    "library", PackageType::StaticLibrary },
    {     "dynlib", PackageType::SharedLibrary },
    {    "headers",       PackageType::Headers },
  };

  auto const find = mapping.find(s);
  if (find == mapping.end())
    return std::nullopt;
  return find->second;
}

auto
get_config_file(
  ToplevelOptions const&                                     options,
  std::optional<std::reference_wrapper<TargetTriplet const>> target_opt,
  std::filesystem::path path) -> std::optional<ConfigurationFile>
{
  if (not std::filesystem::exists(path))
    return std::nullopt;
  std::string config_filedata = read_file(path);
  scl::file   file(config_filedata);

  ConfigurationFile conf;
  scl::deserialize(conf, file);

  SemVer const this_hewg_semver = this_hewg_version;
  auto const   requested_semver = parse_semver(conf.meta.hewg_version);

  if (not requested_semver)
    throw std::runtime_error("invalid requested hewg version");

  if (*requested_semver > this_hewg_semver) {
    if (not options.force) {
      throw std::runtime_error(
        std::format("hewg project requests version {}, but we have {}",
                    *requested_semver,
                    this_hewg_semver));
    }

    threadsafe_print(
      "forcing execution of build system on mismatched hewg version!");
  }

  auto const& name = conf.project.name;
  if (not check_valid_project_identifier(name))
    throw std::runtime_error(
      std::format("project name <{}> is invalid; it must be alphanumeric "
                  "including underscores",
                  name));

  /* get target/release build specific information */
  if (target_opt) {
    auto const& target = target_opt->get();

    auto const& cxx_target_table_name
      = std::format("cxx.target.{}", target.to_string());
    auto const& c_target_table_name
      = std::format("c.target.{}", target.to_string());

    if (file.table_exists(cxx_target_table_name)) {
      CXXTargetConf target_conf;
      scl::deserialize(target_conf, file, cxx_target_table_name);
      conf.cxx.sources.insert(conf.cxx.sources.end(),
                              target_conf.source_files.begin(),
                              target_conf.source_files.end());
    }

    if (file.table_exists(c_target_table_name)) {
      CTargetConf target_conf;
      scl::deserialize(target_conf, file, c_target_table_name);
      conf.c.sources.insert(conf.c.sources.end(),
                            target_conf.source_files.begin(),
                            target_conf.source_files.end());
    }
  }

  return conf;
}

ToolProfile
get_default_tool_profile()
{
#ifdef __linux__
  return ToolProfile{ "linux-gcc" };
#else
#  error building only allowed on linux
#endif
};

jayson::val
config_to_project_manifest(ConfigurationFile const&)
{
  throw std::runtime_error("config to project manifest unimplemented");
}
