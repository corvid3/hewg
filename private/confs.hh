#pragma once

#include <crow.jayson/jayson.hh>
#include <crow.scl/scl.hh>
#include <optional>
#include <string>
#include <tuple>

enum class PackageType : char {
  Executable,
  StaticLibrary,
  SharedLibrary,

  // header only libraries
  Headers,
};

auto project_type_to_string(PackageType) -> std::string_view;
auto project_type_from_string(std::string_view) -> std::optional<PackageType>;

struct ProjectTypeEnumDescriptorJayson {
  static auto
  deserialize(std::string_view what)
  {
    return project_type_from_string(what);
  }

  static auto
  serialize(PackageType const what)
  {
    return project_type_to_string(what);
  }
};

using ProjectTypeEnumDescriptor
  = scl::enum_field_descriptor<PackageType,
                               project_type_from_string,
                               project_type_to_string>;

struct MetaConf {
  // hewg version
  std::string hewg_version;
  PackageType type;

  std::optional<std::string> profile_override;

  using scl_fields = std::tuple<
    scl::field<&MetaConf::hewg_version, "version">,
    scl::enum_field<&MetaConf::type, "type", ProjectTypeEnumDescriptor>,
    scl::field<&MetaConf::profile_override, "profile_override", false>>;
};

struct ProjectConf {
  std::string              version;
  std::string              name;
  std::string              org;
  std::string              description;
  std::vector<std::string> authors;

  using scl_fields
    = std::tuple<scl::field<&ProjectConf::version, "version">,
                 scl::field<&ProjectConf::name, "name">,
                 scl::field<&ProjectConf::org, "org">,
                 scl::field<&ProjectConf::description, "description">,
                 scl::field<&ProjectConf::authors, "authors">>;

  using jayson_fields
    = std::tuple<jayson::obj_field<"version", &ProjectConf::version>,
                 jayson::obj_field<"name", &ProjectConf::name>,
                 jayson::obj_field<"description", &ProjectConf::description>,
                 jayson::obj_field<"authors", &ProjectConf::authors>>;
};

/* for target/release-dependent build flags/sources, see confs.cc */
struct CXXConf {
  std::optional<int>       std;
  std::vector<std::string> flags;
  std::vector<std::string> sources;

  using scl_fields = std::tuple<scl::field<&CXXConf::std, "std", false>,
                                scl::field<&CXXConf::flags, "flags">,
                                scl::field<&CXXConf::sources, "sources">>;
};

struct CConf {
  std::optional<int>       std;
  std::vector<std::string> flags;
  std::vector<std::string> sources;

  using scl_fields = std::tuple<scl::field<&CConf::std, "std", false>,
                                scl::field<&CConf::flags, "flags">,
                                scl::field<&CConf::sources, "sources">>;
};

struct HooksConf {
  std::vector<std::string> once;
  std::vector<std::string> always;

  using scl_fields
    = std::tuple<scl::field<&HooksConf::once, "once", false>,
                 scl::field<&HooksConf::always, "always", false>>;
};

struct ToolProfile {
  std::string tool_profile_name;
  using scl_fields
    = std::tuple<scl::field<&ToolProfile::tool_profile_name, "name">>;
};

struct DependenciesConf {
  std::vector<std::string> internal;
  std::vector<std::string> external;
  std::vector<std::string> system_libraries;

  using scl_fields = std::tuple<
    scl::field<&DependenciesConf::internal, "internal">,
    scl::field<&DependenciesConf::external, "external">,
    scl::field<&DependenciesConf::system_libraries, "system", false>>;
};

auto
get_default_tool_profile() -> ToolProfile;

struct ConfigurationFile {
  MetaConf    meta;
  ProjectConf project;
  ToolProfile tools = get_default_tool_profile();

  // modified with build profile
  CConf   c;
  CXXConf cxx;

  DependenciesConf depends;

  HooksConf prebuild_hooks;
  HooksConf postbuild_hooks;

  using scl_recurse = std::tuple<
    scl::field<&ConfigurationFile::meta, "hewg">,
    scl::field<&ConfigurationFile::project, "project">,
    scl::field<&ConfigurationFile::tools, "tools", false>,
    scl::field<&ConfigurationFile::c, "c">,
    scl::field<&ConfigurationFile::cxx, "cxx">,
    scl::field<&ConfigurationFile::depends, "depends">,
    scl::field<&ConfigurationFile::prebuild_hooks, "hooks.prebuild", false>,
    scl::field<&ConfigurationFile::postbuild_hooks, "hooks.postbuild", false>>;
};
