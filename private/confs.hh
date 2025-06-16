#pragma once

#include <filesystem>
#include <jayson.hh>
#include <scl.hh>
#include <string>
#include <tuple>

using version_triplet = std::tuple<int, int, int>;

inline bool
operator==(version_triplet const l, version_triplet const r)
{
  auto const [lx, ly, lz] = l;
  auto const [rx, ry, rz] = r;

  return lx == rx and ly == ry and lz == rz;
}

inline bool
operator<(version_triplet const l, version_triplet const r)
{
  auto const [lx, ly, lz] = l;
  auto const [rx, ry, rz] = r;

  return lx < rx and ly < ry and lz < rz;
}

std::string inline version_triplet_to_string(version_triplet const t)
{
  auto const [x, y, z] = t;
  return std::format("{}.{}.{}", x, y, z);
}

enum class ProjectType
{
  Executable,
  StaticLibrary,
  SharedLibrary,

  // header only libraries
  Headers,
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

  using scl_fields = std::tuple<
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
    std::tuple<scl::field<&ProjectConf::version, "version">,
               scl::field<&ProjectConf::name, "name">,
               scl::field<&ProjectConf::description, "description">,
               scl::field<&ProjectConf::authors, "authors">>;

  using jayson_fields =
    std::tuple<jayson::obj_field<"version", &ProjectConf::version>,
               jayson::obj_field<"name", &ProjectConf::name>,
               jayson::obj_field<"description", &ProjectConf::description>,
               jayson::obj_field<"authors", &ProjectConf::authors>>;
};

struct FlagsConf
{
  std::vector<std::string> cxx_flags;
  std::vector<std::string> c_flags;
  std::vector<std::string> ld_flags;

  using scl_fields = std::tuple<scl::field<&FlagsConf::cxx_flags, "cxx", false>,
                                scl::field<&FlagsConf::c_flags, "c", false>,
                                scl::field<&FlagsConf::ld_flags, "ld", false>>;
};

struct FilesConf
{
  std::vector<std::string> source;

  using scl_fields = std::tuple<scl::field<&FilesConf::source, "source">>;
};

struct ToolsConf
{
  std::optional<std::string> cxx_tool;
  std::optional<std::string> c_tool;
  std::optional<std::string> ld_tool;

  using scl_fields = std::tuple<scl::field<&ToolsConf::cxx_tool, "cxx", false>,
                                scl::field<&ToolsConf::c_tool, "c", false>,
                                scl::field<&ToolsConf::ld_tool, "ld", false>>;
};

using PackageDep = std::tuple<std::string, version_triplet>;

struct LibraryConf
{
  std::vector<PackageDep> packages = {};
  std::vector<std::string> native = {};

  using scl_fields =
    std::tuple<scl::field<&LibraryConf::packages, "packages", false>,
               scl::field<&LibraryConf::native, "native", false>>;
};

struct HooksConf
{
  std::vector<std::string> once;
  std::vector<std::string> always;

  using scl_fields =
    std::tuple<scl::field<&HooksConf::once, "once", false>,
               scl::field<&HooksConf::always, "always", false>>;
};

struct ConfigurationFile
{
  MetaConf meta;
  ProjectConf project;

  ToolsConf tools;
  FlagsConf flags;

  LibraryConf libs;
  FilesConf files;

  HooksConf prebuild_hooks;
  HooksConf postbuild_hooks;

  using scl_recurse = std::tuple<
    scl::field<&ConfigurationFile::meta, "hewg">,
    scl::field<&ConfigurationFile::project, "project">,
    scl::field<&ConfigurationFile::tools, "tools", false>,
    scl::field<&ConfigurationFile::libs, "libraries", false>,
    scl::field<&ConfigurationFile::flags, "flags">,
    scl::field<&ConfigurationFile::files, "files">,
    scl::field<&ConfigurationFile::prebuild_hooks, "hooks.prebuild", false>,
    scl::field<&ConfigurationFile::postbuild_hooks, "hooks.postbuild", false>>;
};

ConfigurationFile
get_config_file(std::filesystem::path path, std::string_view build_profile);

constexpr auto LINUX_BUILD_PROFILE = "linux";
constexpr auto MINGW_BUILD_PROFILE = "mingw";
