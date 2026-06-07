#include "common.hh"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <compare>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <mutex>
#include <print>
#include <pwd.h>
#include <regex>
#include <thread>
#include <unistd.h>

void
create_directory_checked(std::filesystem::path const& what)
{
  if (not std::filesystem::exists(what))
    std::filesystem::create_directories(what);
  if (not std::filesystem::is_directory(what))
    throw std::runtime_error(std::format(
      "{} must be a directory", std::filesystem::relative(what).string()));
}

auto
hsv_to_rgb(double degrees) -> std::tuple<int, int, int>
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

  std::println("rgb bug");
  return { 0, 0, 0 };
};

namespace
{

auto
epsilon_float(float const lhs, float const rhs) -> bool
{
  auto constexpr epsilon = 0.01;
  return std::abs(lhs - rhs) < epsilon;
}

}
auto
compare_ascii(std::string_view lhs, std::string_view rhs)
  -> std::strong_ordering
{
  if (lhs.size() < rhs.size())
    return std::strong_ordering::less;

  if (lhs.size() > rhs.size())
    return std::strong_ordering::greater;

  for (auto i = 0; i < (int) lhs.size(); i++) {
    auto const l = lhs[i];
    auto const r = rhs[i];
    if (l < r)
      return std::strong_ordering::less;
    if (l > r)
      return std::strong_ordering::greater;
  }

  return std::strong_ordering::equal;
};

auto
split_by_delim(std::string_view const in, char const delim)
  -> std::vector<std::string_view>
{
  std::vector<std::string_view> out;
  size_t                        count{};

  do {
    auto const found = in.substr(count, in.find(delim));
    out.push_back(found);
    count += found.size();
  } while (count != in.size());

  return out;
}

// home path (hopefully...) wont change across an invocation
// of hewg, so a bit of memoization goes on here
auto
get_home_directory() -> std::filesystem::path const&
{
  static std::filesystem::path home_path;
  static std::once_flag        once;

  static auto const func = []() {
#ifdef __linux__
    auto* const home_env = getenv("HOME");
    if (home_env != nullptr)
      return std::filesystem::absolute(std::filesystem::path(home_env));

    auto* const pw_dir = getpwuid(getuid())->pw_dir;
    if (pw_dir != nullptr)
      return std::filesystem::absolute(std::filesystem::path(pw_dir));

    throw std::runtime_error("unable to get home directory");
#else
#  error "hewg does not support systems other than linux"
#endif
  };

  std::call_once(once, [&]() {
    home_path = func();
  });

  return home_path;
}

auto
is_subpathed_by(std::filesystem::path const& owning_directory,
                std::filesystem::path const& child) -> bool
{
  if (not std::filesystem::is_directory(owning_directory)) {
    throw std::runtime_error(
      std::format("is_subpathed_by is given a non-directory as owner, owner: "
                  "<{}>, child: <{}>",
                  owning_directory.string(),
                  child.string()));
  }

  // TODO: this doesn't work if owning_directory has a trailing /
  // because it introduces a new empty component at the end...
  // hmm

  auto const owning_directory_full
    = std::filesystem::absolute(owning_directory);
  auto const child_full = std::filesystem::absolute(child);

  auto const m = std::ranges::mismatch(owning_directory_full, child_full);
  return m.in1 == owning_directory_full.end();
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

auto
check_valid_project_identifier(std::string_view name) -> bool
{
  static std::regex name_validation("[a-zA-Z0-9\\_\\-]+");

  return std::regex_search(name.begin(), name.end(), name_validation);
}

stacktrace_exception::stacktrace_exception(std::string_view what)
  : m_what(what) {};

auto
stacktrace_exception::what() const noexcept -> char const*
{
  std::stringstream ss;

  std::string st;
  // auto st = std::stacktrace::current();
  ss << st << '\n' << m_what << '\n';

  m_fmtBuf = std::move(ss).str();

  return m_fmtBuf.c_str();
}

void
assert_is_absolute(std::filesystem::path const& path)
{
  if (not path.is_absolute())
    throw stacktrace_exception("path provided is not absolute");
}

void
assert_is_subpathed(std::filesystem::path const& parent,
                    std::filesystem::path const& child)
{
  if (not is_subpathed_by(parent, child)) {
    throw stacktrace_exception(
      std::format("provided directory {} is not a child of {}",
                  child.string(),
                  parent.string()));
  }
}
