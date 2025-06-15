#include "common.hh"
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <pwd.h>
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
