#pragma once

#include "cmdline.hh"
#include <filesystem>
#include <jayson.hh>
#include <optional>
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

struct Dependency
{
  std::string name;
  version_triplet version;
  bool exact = false;

  using scl_fields = std::tuple<scl::field<&Dependency::name, "name">,
                                scl::field<&Dependency::version, "version">,
                                scl::field<&Dependency::exact, "exact", false>>;
};

struct MetaConf
{
  version_triplet version;
  ProjectType type;

  std::optional<std::string> profile_override;

  using scl_fields = std::tuple<
    scl::field<&MetaConf::version, "version">,
    scl::enum_field<&MetaConf::type, "type", ProjectTypeEnumDescriptor>,
    scl::field<&MetaConf::profile_override, "profile_override", false>>;
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

struct CXXConf
{
  std::optional<int> std;
  std::vector<std::string> flags;
  std::vector<std::string> sources;

  using scl_fields = std::tuple<scl::field<&CXXConf::std, "std", false>,
                                scl::field<&CXXConf::flags, "flags">,
                                scl::field<&CXXConf::sources, "sources">>;
};

struct CConf
{
  std::optional<int> std;
  std::vector<std::string> flags;
  std::vector<std::string> sources;

  using scl_fields = std::tuple<scl::field<&CConf::std, "std", false>,
                                scl::field<&CConf::flags, "flags">,
                                scl::field<&CConf::sources, "sources">>;
};

struct LibraryConf
{
  std::vector<std::string> native = {};

  using scl_fields =
    std::tuple<scl::field<&LibraryConf::native, "native", false>>;
};

struct HooksConf
{
  std::vector<std::string> once;
  std::vector<std::string> always;

  using scl_fields =
    std::tuple<scl::field<&HooksConf::once, "once", false>,
               scl::field<&HooksConf::always, "always", false>>;
};

struct ToolProfile
{
  std::string tool_profile_name;
  using scl_fields =
    std::tuple<scl::field<&ToolProfile::tool_profile_name, "name">>;
};

ToolProfile
get_default_tool_profile();

struct ConfigurationFile
{
  MetaConf meta;
  ProjectConf project;
  ToolProfile tools = get_default_tool_profile();

  // modified with build profile
  CConf c;
  CXXConf cxx;

  LibraryConf libs;

  std::vector<Dependency> internal_deps;
  std::vector<Dependency> external_deps;

  HooksConf prebuild_hooks;
  HooksConf postbuild_hooks;

  using scl_recurse = std::tuple<
    scl::field<&ConfigurationFile::meta, "hewg">,
    scl::field<&ConfigurationFile::project, "project">,
    scl::field<&ConfigurationFile::libs, "libraries", false>,
    scl::field<&ConfigurationFile::tools, "tools", false>,
    scl::field<&ConfigurationFile::c, "c">,
    scl::field<&ConfigurationFile::cxx, "cxx">,
    scl::field<&ConfigurationFile::internal_deps, "internal", false>,
    scl::field<&ConfigurationFile::external_deps, "external", false>,
    scl::field<&ConfigurationFile::prebuild_hooks, "hooks.prebuild", false>,
    scl::field<&ConfigurationFile::postbuild_hooks, "hooks.postbuild", false>>;
};

struct ToolFile
{
  std::string cxx;
  std::string cc;
  std::string ld;
  std::string ar;

  using scl_fields = std::tuple<scl::field<&ToolFile::cxx, "cxx">,
                                scl::field<&ToolFile::cc, "cc">,
                                scl::field<&ToolFile::ld, "ld">,
                                scl::field<&ToolFile::ar, "ar">>;
};

ConfigurationFile
get_config_file(ToplevelOptions const&,
                std::filesystem::path path,
                std::string_view build_profile);

ToolFile
get_tool_file(ConfigurationFile const& confs, std::string_view build_profile);
