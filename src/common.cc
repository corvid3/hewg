#include "common.hh"
#include "thread_pool.hh"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <functional>
#include <mutex>
#include <print>
#include <pwd.h>
#include <regex>
#include <thread>
#include <unistd.h>

std::vector<std::string_view>
split_by_delim(std::string_view const in, char const delim)
{
  std::vector<std::string_view> out;
  size_t count{};

  do {
    auto const found = in.substr(count, in.find(delim));
    out.push_back(found);
    count += found.size();
  } while (count != in.size());

  return out;
}

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
  if (skip_countdown)
    return;

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

bool
epsilon_float(float const lhs, float const rhs)
{
  return std::abs(lhs - rhs) < 0.01;
}

std::tuple<int, int, int>
hsv_to_rgb(float degrees)
{
  while (degrees >= 360)
    degrees -= 360;

  if (degrees < 0)
    throw std::runtime_error("negative value given to hsv_to_rgb");

  // =x, +y, 0
  if (degrees < 60)
    return { 255, std::lerp(0, 255, degrees / 60), 0 };

  // -x, =y, 0
  if (degrees < 120)
    return { std::lerp(255, 0, (degrees - 60) / 60), 255, 0 };

  // 0, =y, +z
  if (degrees < 180)
    return { 0, 255, std::lerp(0, 255, (degrees - 120) / 60) };

  // 0, -y, =z
  if (degrees < 240)
    return { 0, std::lerp(255, 0, (degrees - 180) / 60), 255 };

  // +x, 0, =z
  if (degrees < 300)
    return { std::lerp(0, 255, (degrees - 240) / 60), 0, 255 };

  // =x, 0, -z
  if (degrees <= 360)
    return { 255, 0, std::lerp(255, 0, (degrees - 300) / 60) };

  std::println("hsv_to_rgb bug");
  return { 0, 0, 0 };
};

// std::string
// get_color_by_thread_id()
// {
//   if (thread_id == MAIN_THREAD_ID)
//     return "\x1b[39;49m";

//   // auto const static num_threads = std::thread::hardware_concurrency();

//   // auto const pct = (thread_id + 1) / num_threads;

//   // auto const [r, g, b] = hsv_to_rgb(300. / pct);
//   return {};

//   // return std::format("\x1b[38;2;{};{};{}m",
//   //                    (unsigned char)r,
//   //                    (unsigned char)g,
//   //                    (unsigned char)b);
// }
