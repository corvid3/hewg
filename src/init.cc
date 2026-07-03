#include "app.hh"
#include "cmdline.hh"
#include "common.hh"
#include "confs.hh"
#include "init.hh"
#include "packages.hh"

#include <filesystem>
#include <fstream>
#include <print>
#include <regex>
#include <string_view>

namespace
{

auto const scl_template = R"([hewg]
version = "%HEWG-VERSION%"
type = "%PROJECT-TYPE%"

[project]
version = "0.0.0"
org = "%ORG%"
name = "%NAME%"
description = "PUT YOUR DESCRIPTION HERE!"
authors = { }

[depends]
internal = { }
external = { }

[cxx]
flags = { "-Wall" "-Wextra" "-Werror" }
std = 23
sources = { }

[c]
flags = { "-Wall" "-Wextra" "-Werror" }
std = 17
sources = { }

[hooks.prebuild]
[hooks.postbuild]
)";

auto const gitignore_template = R"(
.hcache
.cache/
target/
compile_commands.json
)";

std::regex const hewg_version_regex("%HEWG-VERSION%");
std::regex const project_type_regex("%PROJECT-TYPE%");
std::regex const org_regex("%ORG%");
std::regex const name_regex("%NAME%");

void
common_init(std::filesystem::path const& install_directory)
{
  std::filesystem::create_directory(install_directory / "src");
  std::filesystem::create_directory(install_directory / "csrc");
  std::filesystem::create_directory(install_directory / "include");
  std::filesystem::create_directory(install_directory / "private");
  std::filesystem::create_directory(install_directory / "hooks");
}

auto
create_scl_file(std::string_view org,
                std::string_view name,
                std::string_view type) -> std::string
{
  // wow the stdlib regex blows
  std::string str;
  str = std::regex_replace(
    scl_template, hewg_version_regex, std::format("{}", this_hewg_version));
  str = std::regex_replace(str, project_type_regex, std::string(type));
  str = std::regex_replace(str, org_regex, std::string(org));
  str = std::regex_replace(str, name_regex, std::string(name));

  return str;
}

void
check_or_create_directory(AppContext const&            ctx,
                          std::filesystem::path const& directory)
{
  if (not std::filesystem::exists(directory)) {
    std::filesystem::create_directory(directory);
  } else if (not std::filesystem::is_directory(directory)) {
    throw std::runtime_error(std::format(
      "provided path <{}> is not a directory!", directory.string()));
  } else {
    if (not ctx.options().force and not std::filesystem::is_empty(directory))
      throw std::runtime_error(std::format(
        "provided directory <{}> is not empty!", directory.string()));
  }
}

}

void
init(AppContext const& ctx)
{
  if (ctx.init_options().help) {
    terse::print_usage<InitOptions>();
    return;
  }

  if (ctx.bares().size() != 2) {
    throw std::runtime_error(
      "init requires two arguments, the project type "
      "followed by the project name.");
  }

  auto const  project_type  = project_type_from_string(ctx.bares()[0]);
  auto const& project_ident = ctx.bares()[1];

  auto const project_org  = project_ident.substr(0, project_ident.find('.'));
  auto const project_name = project_ident.substr(project_ident.find('.') + 1);

  if (project_org.empty() or project_name.empty())
    throw std::runtime_error("project identifier shall be {ORG}.{NAME}");

  if (not std::regex_match(
        project_org.begin(), project_org.end(), regexes::org))
    throw std::runtime_error("project org provided is not valid");

  if (not std::regex_match(
        project_name.begin(), project_name.end(), regexes::name))
    throw std::runtime_error("project name provided is not valid");

  if (not project_type) {
    throw std::runtime_error(
      "project type provided is not valid; it must be one of <executable>, "
      "<library>, <dynlib>, <headers>");
  }

  auto const install_directory = ctx.init_options()
                                   .directory
                                   .transform([](auto const& in) {
                                     return std::filesystem::path(in);
                                   })
                                   .value_or(std::filesystem::current_path());

  threadsafe_print(
    std::format("initializing project in <{}>...", install_directory.string()));
  do_terminal_countdown(3);
  check_or_create_directory(ctx, install_directory);
  common_init(install_directory);

  switch (*project_type) {
  case PackageType::Executable:
    {
      auto const file
        = create_scl_file(project_org, project_name, "executable");
      std::ofstream(install_directory / "hewg.scl") << file;
    }
    break;

  case PackageType::StaticLibrary:
    {
      auto const file = create_scl_file(project_org, project_name, "library");
      std::ofstream(install_directory / "hewg.scl") << file;
    }
    break;

  case PackageType::SharedLibrary:
    {
      auto const file = create_scl_file(project_org, project_name, "dynlib");
      std::ofstream(install_directory / "hewg.scl") << file;
    }
    break;

  case PackageType::Headers:
    {
      auto const file = create_scl_file(project_org, project_name, "headers");
      std::ofstream(install_directory / "hewg.scl") << file;
    }
    break;
  }

  std::ofstream(install_directory / ".gitignore") << gitignore_template;
}
