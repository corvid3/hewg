#pragma once

#include <scl.hh>
#include <string>
#include <tuple>

using version_triplet = std::tuple<int, int, int>;

enum class ProjectType
{
  Executable,
  StaticLibrary,
  SharedLibrary,
};

std::string_view project_type_to_string(ProjectType);
std::optional<ProjectType> project_type_from_string(std::string_view);

using ProjectTypeEnumDescriptor =
  scl::enum_field_descriptor<ProjectType,
                             project_type_from_string,
                             project_type_to_string>;

struct MetaConf
{
  version_triplet version;
  ProjectType type;

  using scl_fields = scl::field_descriptor<
    scl::field<&MetaConf::version, "version">,
    scl::enum_field<&MetaConf::type, "type", ProjectTypeEnumDescriptor>>;
};

struct ProjectConf
{
  version_triplet version;
  std::string name;
  std::string description;
  std::vector<std::string> authors;

  using scl_fields =
    scl::field_descriptor<scl::field<&ProjectConf::version, "version">,
                          scl::field<&ProjectConf::name, "name">,
                          scl::field<&ProjectConf::description, "description">,
                          scl::field<&ProjectConf::authors, "authors">>;
};

struct FlagsConf
{
  std::vector<std::string> cxx_flags;
  std::vector<std::string> c_flags;
  std::vector<std::string> ld_flags;

  using scl_fields =
    scl::field_descriptor<scl::field<&FlagsConf::cxx_flags, "cxx">,
                          scl::field<&FlagsConf::c_flags, "c">,
                          scl::field<&FlagsConf::ld_flags, "ld">>;
};

struct FilesConf
{
  std::vector<std::string> source;

  using scl_fields =
    scl::field_descriptor<scl::field<&FilesConf::source, "source">>;
};

struct ConfigurationFile
{
  MetaConf meta;
  ProjectConf project;
  FlagsConf flags;
  FilesConf files;

  using scl_recurse =
    scl::field_descriptor<scl::field<&ConfigurationFile::meta, "hewg">,
                          scl::field<&ConfigurationFile::project, "project">,
                          scl::field<&ConfigurationFile::flags, "flags">,
                          scl::field<&ConfigurationFile::files, "files">>;
};

ConfigurationFile
get_config_file();
