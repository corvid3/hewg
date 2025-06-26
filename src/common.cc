#include "common.hh"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <mutex>
#include <pwd.h>
#include <regex>
#include <thread>
#include <unistd.h>

void
create_directory_checked(std::filesystem::path const what)
{
  if (not std::filesystem::exists(what))
    std::filesystem::create_directories(what);
  if (not std::filesystem::is_directory(what))
    throw std::runtime_error(std::format(
      "{} must be a directory", std::filesystem::relative(what).string()));
}

// home path (hopefully...) wont change across an invocation
// of hewg, so a bit of memoization goes on here
std::filesystem::path const&
get_home_directory()
{
  static std::filesystem::path home_path;
  static std::once_flag once;

  static auto const func = []() {
#ifdef __linux__
    auto const home_env = getenv("HOME");
    if (home_env != nullptr)
      return std::filesystem::absolute(std::filesystem::path(home_env));

    auto const pw_dir = getpwuid(getuid())->pw_dir;
    if (pw_dir != nullptr)
      return std::filesystem::absolute(std::filesystem::path(pw_dir));

    throw std::runtime_error("unable to get home directory");
#else
#error "hewg does not support systems other than linux"
#endif
  };

  std::call_once(once, [&]() { home_path = func(); });

  return home_path;
}

bool
is_subpathed_by(std::filesystem::path const owning_directory,
                std::filesystem::path const child)
{
  if (not std::filesystem::is_directory(owning_directory))
    throw std::runtime_error(
      std::format("is_subpathed_by is given a non-directory as owner, owner: "
                  "<{}>, child: <{}>",
                  owning_directory.string(),
                  child.string()));

  // TODO: this doesn't work if owning_directory has a trailing /
  // because it introduces a new empty component at the end...
  // hmm

  auto const owning_directory_full =
    std::filesystem::absolute(owning_directory);
  auto const child_full = std::filesystem::absolute(child);

  auto const m = std::mismatch(owning_directory_full.begin(),
                               owning_directory_full.end(),
                               child_full.begin(),
                               child_full.end());

  return m.first == owning_directory_full.end();
}

void
do_terminal_countdown(int const num)
{
  for (auto i = 0; i < num; i++) {
    threadsafe_print(std::format("{}...\n", num - i));

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

bool
check_valid_project_identifier(std::string_view name)
{
  static std::regex name_validation("[a-zA-Z0-9\\_\\-]+");

  return std::regex_search(name.begin(), name.end(), name_validation);
}
