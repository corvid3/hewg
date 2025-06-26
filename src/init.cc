#include <filesystem>
#include <fstream>
#include <print>
#include <regex>
#include <string_view>
#include <terse.hh>

#include "cmdline.hh"
#include "common.hh"
#include "confs.hh"
#include "init.hh"

auto static const scl_template = R"([hewg]
version = { %VERSION% }
type = "%TYPE%"

[project]
version = { 0 0 0 }
name = "%NAME%"
description = ""
authors = { }

[libraries]
native = { }

[cxx]
flags = { "-Wextra" "-Werror" }
std = 23
sources = 
{
  "%DEFAULTFILE%"  
}

[c]
flags = { "-Wextra" "-Werror" }
std = 17
sources = { }

[hooks.prebuild]
[hooks.postbuild]

)";

std::regex static const version_regex("%VERSION%");
std::regex static const name_regex("%NAME%");
std::regex static const type_regex("%TYPE%");
std::regex static const defaultfile_regex("%DEFAULTFILE%");

static void
common_init(std::filesystem::path install_directory)
{
  std::println("{}", 2);

  std::filesystem::create_directory(install_directory / "src");
  std::filesystem::create_directory(install_directory / "csrc");
  std::filesystem::create_directory(install_directory / "include");
  std::filesystem::create_directory(install_directory / "private");
  std::filesystem::create_directory(install_directory / "hooks");
}

static std::string
create_scl_file(std::string name,
                std::string type,
                std::string default_filename)
{
  // wow the stdlib regex blows
  auto&& a = std::regex_replace(scl_template,
                                version_regex,
                                std::format("{} {} {}",
                                            std::get<0>(this_hewg_version),
                                            std::get<1>(this_hewg_version),
                                            std::get<2>(this_hewg_version)));
  auto&& b = std::regex_replace(a, name_regex, name);
  auto&& c = std::regex_replace(b, type_regex, type);
  auto&& d = std::regex_replace(c, defaultfile_regex, default_filename);

  return d;
}

static void
init_executable(std::filesystem::path install_directory,
                std::string_view project_name)
{
  auto const file =
    create_scl_file(std::string(project_name), "executable", "main.cc");

  auto const static main_filedata = R"(#include<iostream>

int main() {
  std::cout << "hello, world!\n";  
}
)";

  std::ofstream(install_directory / "hewg.scl") << file;
  std::ofstream(install_directory / "src" / "main.cc") << main_filedata;
}

static void
init_library(std::filesystem::path install_directory,
             std::string_view project_name)
{
  auto const file = create_scl_file(std::string(project_name), "library", "");

  std::ofstream(install_directory / "hewg.scl") << file;
}

static void
init_shared(std::filesystem::path install_directory,
            std::string_view project_name)
{
  auto const file = create_scl_file(std::string(project_name), "dynlib", "");

  std::ofstream(install_directory / "hewg.scl") << file;
}

static void
init_headers(std::filesystem::path install_directory,
             std::string_view project_name)
{
  auto const file = create_scl_file(std::string(project_name), "headers", "");

  std::ofstream(install_directory / "hewg.scl") << file;
  std::ofstream(install_directory / "include" /
                std::format("{}.hh", project_name))
    << "";
}

static void
check_or_create_directory(std::filesystem::path directory)
{
  if (not std::filesystem::exists(directory)) {
    std::filesystem::create_directory(directory);
  } else if (not std::filesystem::is_directory(directory)) {
    throw std::runtime_error(std::format(
      "provided path <{}> is not a directory!", directory.string()));
  } else {
    if (not std::filesystem::is_empty(directory))
      throw std::runtime_error(std::format(
        "provided directory <{}> is not empty!", directory.string()));
  }
}

void
init(InitOptions const& options, std::span<std::string const> bares)
{
  auto const this_dir = std::filesystem::current_path();

  if (options.help) {
    terse::print_usage<InitOptions>();
    return;
  }

  if (bares.size() != 2)
    throw std::runtime_error("init requires two arguments, the project type "
                             "followed by the project name.");

  auto const project_type = project_type_from_string(bares[0]);
  auto const project_name = bares[1];

  if (not check_valid_project_identifier(project_name))
    throw std::runtime_error("provided project name is not valid, it must fall "
                             "within the regex range [a-zA-Z0-9_]+");

  if (not project_type)
    throw std::runtime_error(
      "project type provided is not valid; it must be one of <executable>, "
      "<library>, <dynlib>, <headers>");

  auto const install_directory =
    options.directory
      .transform([](auto const in) { return std::filesystem::path(in); })
      .value_or(std::filesystem::current_path());

  threadsafe_print(
    std::format("initializing project in <{}>...", install_directory.string()));
  do_terminal_countdown(3);

  check_or_create_directory(install_directory);
  common_init(install_directory);

  switch (*project_type) {
    case ProjectType::Executable:
      init_executable(install_directory, project_name);
      break;

    case ProjectType::StaticLibrary:
      init_library(install_directory, project_name);
      init_library(install_directory, project_name);
      break;

    case ProjectType::SharedLibrary:
      init_shared(install_directory, project_name);
      break;

    case ProjectType::Headers:
      init_headers(install_directory, project_name);
      break;
  }
}
