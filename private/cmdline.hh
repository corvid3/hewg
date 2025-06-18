#pragma once

#include <optional>
#include <terse.hh>
#include <thread>

struct CleanOptions : terse::TerminalSubcommand
{
  constexpr static auto name = "clean";
  constexpr static auto usage =
    "removes all build artifacts from the hewg cache";

  bool help = false;

  using options = std::tuple<
    terse::Option<"help", 'h', "prints this help", &CleanOptions::help>>;
};

struct InitOptions : terse::TerminalSubcommand
{
  constexpr static auto name = "init";
  constexpr static auto usage =
    "initializes a basic hewg project in the current directory";

  bool help = false;

  using options = std::tuple<
    terse::Option<"help", 'h', "prints this help", &InitOptions::help>>;
};

struct InstallOptions : terse::TerminalSubcommand
{
  constexpr static auto name = "install";
  constexpr static auto usage =
    "installs a built hewg project into the users path";

  bool help = false;

  using options = std::tuple<
    terse::Option<"help", 'h', "prints this help", &InstallOptions::help>>;
};

struct BuildOptions : terse::TerminalSubcommand
{
  constexpr static auto name = "build";
  constexpr static auto usage =
    "triggers the hewg build system on the current project";

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
  constexpr static auto usage =
    "C/CXX build system and package manager infrastructure";

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
