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
    std::filesystem::create_directory(what);
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
  static std::regex name_validation("[a-zA-Z0-9\\_]+");

  if (not std::regex_search(name.begin(), name.end(), name_validation))
    return false;

  return true;
}
