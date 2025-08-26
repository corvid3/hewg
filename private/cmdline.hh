#pragma once

#include <crow.terse/terse.hh>
#include <optional>
#include <thread>

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
  bool install = false;
  std::optional<std::string> target = std::nullopt;

  using options = std::tuple<
    terse::Option<"help", 'h', "prints this help", &BuildOptions::help>,
    terse::Option<"release",
                  std::nullopt,
                  "changes the build type to release mode, enabling "
                  "all optimizations and stripping",
                  &BuildOptions::release>,
    terse::Option<"install",
                  std::nullopt,
                  "after building, compiles the built project into a package "
                  "and installs into the local database",
                  &BuildOptions::install>,
    terse::Option<"target",
                  std::nullopt,
                  "sets the target triple to build for",
                  &BuildOptions::target>>;
};

struct ToplevelOptions : terse::NonterminalSubcommand
{
  bool force = false;
  bool skip_pause = false;

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
    terse::Option<"skip",
                  's',
                  "skips the countdown on a number of different commands",
                  &ToplevelOptions::skip_pause>,
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

  using subcommands = std::tuple<BuildOptions, CleanOptions, InitOptions>;
};

decltype(terse::execute<ToplevelOptions>({}, {}))
parse_cmdline(int argc, char** argv);
