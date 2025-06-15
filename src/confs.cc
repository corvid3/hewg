#include <filesystem>
#include <jayson.hh>
#include <optional>
#include <scl.hh>

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
get_config_file(std::filesystem::path path, std::string_view build_profile)
{
  std::string config_filedata = read_file(path);
  scl::file file(config_filedata);

  auto const flags_bp = std::string("flags.").append(build_profile);
  auto const files_bp = std::string("files.").append(build_profile);

  ConfigurationFile conf;
  scl::deserialize(conf, file);

  // now we do some jank where we append vectors
  // depending on the build profile

  if (file.table_exists(flags_bp)) {
    FlagsConf append;
    scl::deserialize(append, file, flags_bp);

    conf.flags.c_flags.insert(
      conf.flags.c_flags.end(), append.c_flags.begin(), append.c_flags.end());

    conf.flags.cxx_flags.insert(conf.flags.cxx_flags.end(),
                                append.cxx_flags.begin(),
                                append.cxx_flags.end());

    conf.flags.ld_flags.insert(conf.flags.ld_flags.end(),
                               append.ld_flags.begin(),
                               append.ld_flags.end());
  }

  if (file.table_exists(files_bp)) {
    FilesConf append;
    scl::deserialize(append, file, files_bp);

    conf.files.source.insert(
      conf.files.source.end(), append.source.begin(), append.source.end());
  }

  return conf;
}

jayson::val
config_to_project_manifest(ConfigurationFile const&)
{
  throw std::runtime_error("config to project manifest unimplemented");

  // jayson::obj out;

  // out.insert_or_assign("name", config.project.name);

  // jayson::array version;

  // out.insert_or_assign("version", config.project.version);

  // return out;
}
