#include <filesystem>
#include <jayson.hh>
#include <optional>
#include <scl.hh>

#include "analysis.hh"
#include "cmdline.hh"
#include "common.hh"
#include "confs.hh"
#include "paths.hh"

std::string_view
project_type_to_string(ProjectType const t)
{
  static const std::map<ProjectType, std::string> mapping = {
    { ProjectType::Executable, "executable" },
    { ProjectType::StaticLibrary, "library" },
    { ProjectType::SharedLibrary, "dynlib" },
    { ProjectType::Headers, "headers" },
  };

  return mapping.find(t)->second;
}

std::optional<ProjectType>
project_type_from_string(std::string_view s)
{
  static const std::map<std::string, ProjectType, std::less<>> mapping = {
    { "executable", ProjectType::Executable },
    { "library", ProjectType::StaticLibrary },
    { "dynlib", ProjectType::SharedLibrary },
    { "headers", ProjectType::Headers },
  };

  auto const find = mapping.find(s);
  if (find == mapping.end())
    return std::nullopt;
  return find->second;
}

ConfigurationFile
get_config_file(ToplevelOptions const& options,
                std::filesystem::path path,
                std::string_view build_profile)
{
  std::string config_filedata = read_file(path);
  scl::file file(config_filedata);

  auto const cxx_bp = std::string("cxx.").append(build_profile);
  auto const c_bp = std::string("c.").append(build_profile);
  auto const tool_bp = std::string("tools.").append(build_profile);

  ConfigurationFile conf;
  scl::deserialize(conf, file);

  if (not semantically_valid(conf.meta.version, this_hewg_version)) {
    if (not options.force)
      throw std::runtime_error(
        std::format("hewg project requests version {}, but we have {}",
                    version_triplet_to_string(conf.meta.version),
                    version_triplet_to_string(this_hewg_version)));
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

  if (file.table_exists(cxx_bp)) {
    CXXConf append;
    scl::deserialize(append, file, cxx_bp);

    conf.cxx.flags = append.flags;
    conf.cxx.sources.insert(
      conf.cxx.sources.end(), append.sources.begin(), append.sources.end());

    if (append.std)
      conf.cxx.std = append.std;
  }

  if (file.table_exists(c_bp)) {
    CConf append;
    scl::deserialize(append, file, c_bp);

    conf.c.flags = append.flags;
    conf.c.sources.insert(
      conf.c.sources.end(), append.sources.begin(), append.sources.end());

    if (append.std)
      conf.c.std = append.std;
  }

  if (file.table_exists(tool_bp)) {
    ToolProfile replace;
    scl::deserialize(replace, file, tool_bp);
    conf.tools = replace;
  }

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

ToolFile
get_tool_file(ConfigurationFile const&, std::string_view build_profile)
{
  auto const static tool_dir = user_hewg_directory / "tools";
  if (build_profile == "default") {
#ifdef __linux__
    auto const tool_file = tool_dir / "linux-gcc";
#else
#error only linux supported at this time
#endif

    ToolFile into;
    auto filedata = read_file(tool_file);
    scl::file file(filedata);
    scl::deserialize(into, file, "tools");

    return into;
  } else {
    throw std::runtime_error("only supporting default build profile rn");
  }
}

jayson::val
config_to_project_manifest(ConfigurationFile const&)
{
  throw std::runtime_error("config to project manifest unimplemented");
}
