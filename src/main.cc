#include "app.hh"
#include "build.hh"
#include "cmdline.hh"
#include "common.hh"
#include "init.hh"
#include "paths.hh"

#include <cassert>
#include <chrono>
#include <cmath>
#include <crow.jayson/jayson.hh>
#include <crow.scl/scl.hh>
#include <crow.terse/terse.hh>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <variant>
#include <vector>

// i don't want to see m'code squandered...
// auto const link_step_messages = { "now, let's get linking..." };

namespace
{

constexpr auto terminal_countdown_length = 5;

void
clean()
{
  if (not std::filesystem::exists(hewg_cache_path))
    return;

  std::vector<std::filesystem::path> to_clean;

  auto const delete_if_exists = [&](std::filesystem::path const& what) {
    if (not std::filesystem::exists(what))
      return;
    to_clean.push_back(what);
  };

  delete_if_exists(hewg_cache_path);

  if (to_clean.empty()) {
    threadsafe_print("nothing to clean!\n");
    return;
  }
  for (auto const& sf : to_clean)
    threadsafe_print(std::format("deleting: {}\n", sf.string()));

  do_terminal_countdown(terminal_countdown_length);

  for (auto const& sf : to_clean)
    std::filesystem::remove_all(sf);
}

void
do_build(AppContext& ctx)
{
  if (ctx.build_options().help) {
    std::cout << terse::print_usage<BuildOptions>() << '\n';
    return;
  }

  build(ctx);
}

void
do_clean(AppContext& ctx)
{
  if (ctx.clean_options().help) {
    std::cout << terse::print_usage<CleanOptions>() << '\n';
    return;
  }

  clean();
}

void
do_init(AppContext const& ctx)
{
  if (ctx.init_options().help) {
    std::cout << terse::print_usage<InitOptions>() << '\n';
    return;
  }

  init(ctx);
}

void
do_package(AppContext const& ctx)

{
  (void) ctx;
  assert(false);

  // auto const& [options, packsubcmds] = terse::get<PackageOptions>(scmds);

  // if (terse::holds<std::monostate>(packsubcmds)) {
  //   std::cout << terse::print_usage<PackageOptions>() << '\n';
  // } else if (terse::holds<PackageSelectOptions>(packsubcmds)) {
  //   if (bares.size() != 1) {
  //     std::println("select expects a single argument, the package
  //     identifier"); return;
  //   }

  //   auto ident = parse_package_identifier(bares[0]);
  //   if (not ident)
  //     throw std::runtime_error(
  //       std::format("invalid package identifier, {}", bares[0]));

  //   auto db = open_package_db();

  //   select_executable(db, *ident);
  // }
}

}

auto
main(int argc, char** argv) -> int
try {
  AppContext ctx(argc, argv);

  // TODO: change this with an argument
  threadsafe_print_verbose(
    std::format("using <{}> tasks\n", ctx.options().num_tasks));

  if (ctx.options().print_version) {
    using namespace std::chrono;
    auto const dur         = duration<long>(_hewg_build_date_package_hewg);
    auto const since_epoch = time_point<utc_clock, seconds>(dur);

    threadsafe_print(std::format("version <{}>\n", this_hewg_version));
    threadsafe_print(std::format("built <{}> UTC\n", since_epoch));

    return 0;
  }

  if (std::holds_alternative<std::monostate>(ctx.options().terse_subcmds)) {
    std::cout << terse::print_usage<ToplevelOptions>() << '\n';
  } else if (std::holds_alternative<BuildOptions>(
               ctx.options().terse_subcmds)) {
    do_build(ctx);
  } else if (std::holds_alternative<CleanOptions>(
               ctx.options().terse_subcmds)) {
    do_clean(ctx);
  } else if (std::holds_alternative<InitOptions>(ctx.options().terse_subcmds)) {
    do_init(ctx);
  } else if (std::holds_alternative<PackageOptions>(
               ctx.options().terse_subcmds)) {
    do_package(ctx);
  }
} catch (std::exception const& e) {
  threadsafe_print("ERROR: ", e.what(), '\n');
  return 0;
}
