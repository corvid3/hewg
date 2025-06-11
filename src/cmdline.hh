#pragma once

#include <terse.hh>
#include <thread>

struct CleanOptions : terse::TerminalSubcommand
{
  constexpr static auto name = terse::comptime_str("clean");
  using options = std::tuple<>;
};

struct InitOptions : terse::TerminalSubcommand
{
  constexpr static auto name = terse::comptime_str("init");
  using options = std::tuple<>;
};

struct BuildOptions : terse::TerminalSubcommand
{
  constexpr static auto name = terse::comptime_str("build");

  bool release = false;

  using options =
    std::tuple<terse::Option<"release",
                             std::nullopt,
                             "changes the build type to release mode, enabling "
                             "all optimizations and stripping",
                             &BuildOptions::release>>;
};

struct ToplevelOptions : terse::NonterminalSubcommand
{
  bool verbose_print = false;
  unsigned num_tasks = std::thread::hardware_concurrency();
  std::optional<std::string> config_file_path;

  using options = std::tuple<
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
                  &ToplevelOptions::config_file_path>>;

  using subcommands = std::tuple<BuildOptions, CleanOptions, InitOptions>;
};

decltype(terse::execute<ToplevelOptions>({}, {}))
parse_cmdline(int argc, char** argv);
