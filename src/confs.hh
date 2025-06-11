#pragma once

#include <filesystem>
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
    scl::field_descriptor<scl::field<&FlagsConf::cxx_flags, "cxx", false>,
                          scl::field<&FlagsConf::c_flags, "c", false>,
                          scl::field<&FlagsConf::ld_flags, "ld", false>>;
};

struct FilesConf
{
  std::vector<std::string> source;

  using scl_fields =
    scl::field_descriptor<scl::field<&FilesConf::source, "source">>;
};

struct ToolsConf
{
  // TODO: update libscl such that it just
  // uses default field initializers, not
  // default values in the scl::field lol
  std::string cxx_tool = "c++";
  std::string c_tool = "cc";
  std::string ld_tool = "ld";

  using scl_fields =
    scl::field_descriptor<scl::field<&ToolsConf::cxx_tool, "cxx">,
                          scl::field<&ToolsConf::c_tool, "c">,
                          scl::field<&ToolsConf::ld_tool, "ld">>;
};

struct LibraryConf
{
  std::vector<std::string> privates = {};

  using scl_fields =
    scl::field_descriptor<scl::field<&LibraryConf::privates, "private", true>>;
};

struct ConfigurationFile
{
  MetaConf meta;
  ProjectConf project;
  ToolsConf tools;
  LibraryConf libs;
  FlagsConf flags;
  FilesConf files;
};

ConfigurationFile
get_config_file(std::filesystem::path path, std::string_view build_profile);

constexpr auto LINUX_BUILD_PROFILE = "linux";
constexpr auto MINGW_BUILD_PROFILE = "mingw";
