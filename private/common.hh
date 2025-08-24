#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "semver.hh"
#include "thread_pool.hh"

using namespace std::string_view_literals;

using version_triplet = std::tuple<int, int, int>;

extern "C" int __hewg_version_package_hewg[3];
extern "C" char const* __hewg_prerelease_package_hewg;
extern "C" char const* __hewg_metadata_package_hewg;
extern "C" long __hewg_build_date_package_hewg;

SemVer const inline this_hewg_version{ 0, 4, 0, std::nullopt, std::nullopt };

template<typename T>
class atomic_vec
{
  std::vector<T> m_vec;
  mutable std::mutex m_mutex;

public:
  atomic_vec() = default;

  template<typename M>
  void push_back(M&& what)
  {
    std::scoped_lock lock(m_mutex);
    m_vec.push_back(what);
  }

  void map(auto func)
  {
    std::scoped_lock lock(m_mutex);
    for (auto& v : m_vec)
      func(v);
  }

  void clear()
  {
    std::scoped_lock lock(m_mutex);
    m_vec.clear();
  }

  auto size() const
  {
    std::scoped_lock lock(m_mutex);
    return m_vec.size();
  }
};

std::strong_ordering compare_ascii(std::string_view, std::string_view);

std::vector<std::string_view>
split_by_delim(std::string_view const, char const delim);

template<typename T, std::convertible_to<T>... As>
auto
make_array(As const&... vals) -> std::array<T, sizeof...(As)>
{
  return std::array{ T(vals)... };
}

constexpr std::size_t
operator""_kb(unsigned long long const in)
{
  return in * 1024;
}

constexpr std::size_t
operator""_mb(unsigned long long const in)
{
  return in * 1024_kb;
}

template<typename L>
auto&
append_vec(std::vector<L>& into, std::ranges::range auto const& rhs)
{
  into.insert(into.end(), rhs.begin(), rhs.end());
  return into;
}

template<typename L>
decltype(auto)
operator+(std::vector<L>&& lhs, std::vector<L>&& rhs)
{
  lhs.insert(
    lhs.end(), std::move_iterator(rhs.begin()), std::move_iterator(rhs.end()));
  return std::move(lhs);
}

template<typename L>
decltype(auto)
operator+(std::vector<L> const& lhs, std::vector<L> const& rhs)
{
  std::vector<L> out = lhs;
  out.insert(out.end(), rhs.begin(), rhs.end());
  return out;
}

inline std::mutex stdout_mutex;

// full saturation
// 0 -> 360
std::tuple<int, int, int>
hsv_to_rgb(float const degrees);

inline std::string
hsv_terminal_colorize(float pct)
{
  auto [r, g, b] = hsv_to_rgb(300. * pct);
  r = std::min(255, r + 50);
  g = std::min(255, g + 50);
  b = std::min(255, b + 50);

  return std::format("\x1b[38;2;{};{};{}m",
                     (unsigned char)r,
                     (unsigned char)g,
                     (unsigned char)b);
};

inline std::string
greyscale_terminal_colorize(float const pct)
{
  int const val = std::min<int>(255, 255 * pct);
  return std::format("\x1b[38;2;{};{};{}m", val, val, val);
}

inline std::string
get_color_by_thread_id()
{
  if (thread_id == MAIN_THREAD_ID)
    return "\x1b[39;49m";

  float const pct = (thread_id) / (float)num_tasks;
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

  std::scoped_lock lock(stdout_mutex);
  std::cout << std::format(
    "{}{:13}\x1b[39;49m| ", get_color_by_thread_id(), thread_fmt);

  std::cout << s;

  // just make sure theres a line ending
  if (not s.ends_with('\n'))
    std::cout << '\n';
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

inline std::string
read_file(std::filesystem::path const p)
{
  std::ifstream f(p);
  if (f.fail())
    throw std::runtime_error(
      std::format("unable to open file <{}>", p.string()));
  return (std::stringstream() << f.rdbuf()).str();
}

bool
is_subpathed_by(std::filesystem::path const owning_directory,
                std::filesystem::path const child);

void
create_directory_checked(std::filesystem::path const what);

std::filesystem::path const&
get_home_directory();

inline bool skip_countdown = false;

// use this if you need to buffer some actions
// on the terminal with time
// use this whenever you're deleting files, probably
void
do_terminal_countdown(int const num);

// used for project name
// regex: [a-zA-Z0-9_]+
bool
check_valid_project_identifier(std::string_view what);
