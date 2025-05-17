#include <optional>

#include "common.hh"
#include "confs.hh"
#include "paths.hh"

std::string_view
project_type_to_string(ProjectType const t)
{
  static const std::map<ProjectType, std::string> mapping = {
    { ProjectType::Executable, "executable" },
    { ProjectType::StaticLibrary, "library" },
    { ProjectType::SharedLibrary, "shared" }
  };

  return mapping.find(t)->second;
}

std::optional<ProjectType>
project_type_from_string(std::string_view s)
{
  static const std::map<std::string, ProjectType, std::less<>> mapping = {
    { "executable", ProjectType::Executable },
    { "library", ProjectType::StaticLibrary },
    { "shared", ProjectType::SharedLibrary },
  };

  auto const find = mapping.find(s);
  if (find == mapping.end())
    return std::nullopt;
  return find->second;
}

ConfigurationFile
get_config_file()
{
  std::string config_filedata = read_file(hewg_config_path);
  scl::scl_file file(config_filedata);
  ConfigurationFile conf;
  scl::deserialize(conf, file);
  return conf;
}
