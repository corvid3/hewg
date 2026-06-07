#pragma once

#include "semver.hh"
#include "thread_pool.hh"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <compare>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using namespace std::string_view_literals;

using version_triplet = std::tuple<int, int, int>;

extern "C" int         _hewg_version_package_hewg[3];
extern "C" char const* _hewg_prerelease_package_hewg;
extern "C" char const* _hewg_metadata_package_hewg;
extern "C" long        _hewg_build_date_package_hewg;

inline auto
nullptr_to_opt(char const* in) -> std::optional<char const*>
{
  if (in == nullptr)
    return std::nullopt;
  return in;
}

inline SemVer const this_hewg_version{
  _hewg_version_package_hewg[0],
  _hewg_version_package_hewg[1],
  _hewg_version_package_hewg[2],
  nullptr_to_opt(_hewg_prerelease_package_hewg),
  nullptr_to_opt(_hewg_metadata_package_hewg)
};

template<typename T>
class atomic_vec
{
  std::vector<T>     m_vec;
  mutable std::mutex m_mutex;

public:
  atomic_vec() = default;

  template<typename M>
  void
  push_back(M&& what)
  {
    std::scoped_lock lock(m_mutex);
    m_vec.push_back(std::forward(what));
  }

  void
  map(auto func)
  {
    std::scoped_lock lock(m_mutex);
    for (auto& v : m_vec)
      func(v);
  }

  void
  clear()
  {
    std::scoped_lock lock(m_mutex);
    m_vec.clear();
  }

  auto
  size() const
  {
    std::scoped_lock lock(m_mutex);
    return m_vec.size();
  }
};

auto compare_ascii(std::string_view, std::string_view) -> std::strong_ordering;

auto
split_by_delim(std::string_view, char delim) -> std::vector<std::string_view>;

template<typename T, std::convertible_to<T>... As>
auto
make_array(As const&... vals) -> std::array<T, sizeof...(As)>
{
  return std::array{ T(vals)... };
}

constexpr auto
operator""_kb(unsigned long long const in) -> std::size_t
{
  constexpr auto kibi = 1024;
  return in * kibi;
}

constexpr auto
operator""_mb(unsigned long long const in) -> std::size_t
{
  constexpr auto mibi = 1024_kb;
  return in * mibi;
}

template<typename L>
auto
append_vec(std::vector<L>& into, std::ranges::range auto const& rhs)
{
  into.insert(into.end(), rhs.begin(), rhs.end());
  return into;
}

template<typename L>
auto
operator+(std::vector<L>&& lhs, std::vector<L> rhs) -> decltype(auto)
{
  lhs.insert(
    lhs.end(), std::move_iterator(rhs.begin()), std::move_iterator(rhs.end()));
  rhs.clear();
  return std::move(lhs);
}

template<typename L>
auto
operator+(std::vector<L> const& lhs, std::vector<L> const& rhs)
  -> decltype(auto)
{
  std::vector<L> out = lhs;
  out.insert(out.end(), rhs.begin(), rhs.end());
  return out;
}

class stacktrace_exception : public std::exception
{
public:
  explicit stacktrace_exception(std::string_view what);

  auto
  what() const noexcept -> char const* override;

private:
  std::string         m_what;
  mutable std::string m_fmtBuf;
};

void
assert_is_absolute(std::filesystem::path const&);

void
assert_is_subpathed(std::filesystem::path const& parent,
                    std::filesystem::path const& child);

inline std::mutex stdout_mutex;

// full saturation
// 0 -> 360
auto
hsv_to_rgb(double) -> std::tuple<int, int, int>;

inline auto
hsv_terminal_colorize(double pct) -> std::string
{
  constexpr auto brightening  = 50;
  constexpr auto degree_range = 300;
  auto [r, g, b]              = hsv_to_rgb(degree_range * pct);
  r                           = std::min(CHAR_MAX, r + brightening);
  g                           = std::min(CHAR_MAX, g + brightening);
  b                           = std::min(CHAR_MAX, b + brightening);

  return std::format("\x1b[38;2;{};{};{}m",
                     (unsigned char) r,
                     (unsigned char) g,
                     (unsigned char) b);
};

inline auto
greyscale_terminal_colorize(double const pct) -> std::string
{
  int const val = std::min<int>(CHAR_MAX, CHAR_MAX * pct);
  return std::format("\x1b[38;2;{};{};{}m", val, val, val);
}

inline auto
get_color_by_thread_id() -> std::string
{
  if (thread_id == MAIN_THREAD_ID)
    return "\x1b[39;49m";

  double const pct = (double) (thread_id) / (double) num_tasks;
  return hsv_terminal_colorize(pct);
}

inline void
threadsafe_print(auto const&... v)
{
  std::stringstream b;
  (b << ... << v);
  std::string const s = std::move(b).str();

  if (s.empty())
    return;

  std::string thread_fmt;
  if (thread_id != MAIN_THREAD_ID)
    thread_fmt = std::format("(thread {})", thread_id + 1);
  else
    thread_fmt = "(hewg)";

  std::unique_lock lock(stdout_mutex);
  auto const       color = get_color_by_thread_id();
  std::cout << std::format("{}{:13}\x1b[39;49m| ", color, thread_fmt);

  std::cout << s;

  // just make sure theres a line ending
  if (not s.ends_with('\n'))
    std::cout << '\n';

  lock.unlock();
}

// this is set to true in main()
// if the verbose flag was set
inline bool verbose_output = false;

inline void
threadsafe_print_verbose(auto const&... v)
{
  if (not verbose_output)
    return;

  threadsafe_print(v...);
}

inline auto
read_file(std::filesystem::path const& p) -> std::string
{
  std::ifstream f(p);
  if (f.fail())
    throw std::runtime_error(
      std::format("unable to open file <{}>", p.string()));
  return (std::stringstream() << f.rdbuf()).str();
}

auto
is_subpathed_by(std::filesystem::path const& owning_directory,
                std::filesystem::path const& child) -> bool;

void
create_directory_checked(std::filesystem::path const& what);

auto
get_home_directory() -> std::filesystem::path const&;

inline bool skip_countdown = false;

// use this if you need to buffer some actions
// on the terminal with time
// use this whenever you're deleting files, probably
void
do_terminal_countdown(int num);

// used for project name
// regex: [a-zA-Z0-9_]+
auto
check_valid_project_identifier(std::string_view name) -> bool;
