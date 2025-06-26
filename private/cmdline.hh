#pragma once

#include <optional>
#include <terse.hh>
#include <thread>

// using namespace std::string_view_literals;

struct CleanOptions : terse::TerminalSubcommand
{
  constexpr static auto name = "clean";
  constexpr static auto usage = "";

  constexpr static auto short_description =
    "removes all build artifacts from the hewg cache";

  constexpr static auto description =
    "Removes any and all build artifacts from the hewg cache, that is object "
    "files and persistent information about the build system. Use this to "
    "force a full recompile of a project.";

  bool help = false;

  using options = std::tuple<
    terse::Option<"help", 'h', "prints this help", &CleanOptions::help>>;
};

struct InitOptions : terse::TerminalSubcommand
{
  constexpr static auto name = "init";
  constexpr static auto usage = "<type> <name>";
  constexpr static auto short_description = "creates a basic hewg project";
  constexpr static auto description =
    "Initializes a basic hewg project in the current directory. Project type "
    "is one of <executable>, <library>, <shared>, or <headers>. A name must be "
    "provided.";

  std::optional<std::string> directory;
  bool help = false;

  using options = std::tuple<
    terse::Option<"help", 'h', "prints this help", &InitOptions::help>,
    terse::Option<
      "directory",
      'd',
      "changes the directory to init in. creates dir if it doesn't exist",
      &InitOptions::directory>>;
};

struct InstallOptions : terse::TerminalSubcommand
{
  constexpr static auto name = "install";
  constexpr static auto usage = "<profile>";
  constexpr static auto short_description =
    "installs a built hewg project into the users path";
  constexpr static auto description =
    "Installs a built hewg project into the users path, depending on build "
    "profile. Build profile defaults to the running operating system. Hewg "
    "will automatically symlink the latest version of an "
    "executable package to the path.";

  bool help = false;

  using options = std::tuple<
    terse::Option<"help", 'h', "prints this help", &InstallOptions::help>>;
};

struct BuildOptions : terse::TerminalSubcommand
{
  constexpr static auto name = "build";
  constexpr static auto usage = "<profile>";
  constexpr static auto short_description =
    "triggers the hewg build system on the current project";
  constexpr static auto description =
    "Builds the current project based off of "
    "the provided build profile. Defaults to the running operating system.";

  bool help = false;
  bool release = false;

  using options = std::tuple<
    terse::Option<"help", 'h', "prints this help", &BuildOptions::help>,
    terse::Option<"release",
                  std::nullopt,
                  "changes the build type to release mode, enabling "
                  "all optimizations and stripping",
                  &BuildOptions::release>>;
};

struct ToplevelOptions : terse::NonterminalSubcommand
{
  bool force = false;

  bool verbose_print = false;
  unsigned num_tasks = std::thread::hardware_concurrency();
  std::optional<std::string> config_file_path;

  bool print_version = false;

  constexpr static auto name = "hewg";
  constexpr static auto usage = "";
  constexpr static auto short_description =
    "C/CXX build system and package manager infrastructure";
  constexpr static auto description =
    "C/CXX build system and package manager infrastructure. Expects a hewg.scl "
    "file in the current directory to do anything.";

  using options = std::tuple<
    terse::Option<"force",
                  std::nullopt,
                  "forces the build system to try to run against a project. "
                  "use this wisely",
                  &ToplevelOptions::force>,
    terse::Option<"verbose",
                  'v',
                  "enables verbose printing & diag output",
                  &ToplevelOptions::verbose_print>,
    terse::Option<"tasks",
                  'j',
                  "provides number of tasks to run for compilation jobs",
                  &ToplevelOptions::num_tasks>,
    terse::Option<"config",
                  'c',
                  "sets the config file to read",
                  &ToplevelOptions::config_file_path>,
    terse::Option<"version",
                  std::nullopt,
                  "prints the version of hewg",
                  &ToplevelOptions::print_version>>;

  using subcommands =
    std::tuple<BuildOptions, CleanOptions, InitOptions, InstallOptions>;
};

decltype(terse::execute<ToplevelOptions>({}, {}))
parse_cmdline(int argc, char** argv);
