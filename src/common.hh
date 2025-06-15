#pragma once

#include "thread_pool.hh"
#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

using namespace std::string_view_literals;

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
void
append_vec(std::vector<L>& into, std::ranges::range auto const& rhs)
{
  std::copy(rhs.begin(), rhs.end(), std::inserter(into, into.end()));
}

static inline std::mutex stdout_mutex;

inline std::string_view
get_color_by_thread_id()
{
  if (thread_id == MAIN_THREAD_ID)
    return "\x1b[39;49m";

  // reasonbly ok way to get
  // a pseudorandom number for each core
  unsigned v = unsigned((std::pow(2, 31) - 1) / (thread_id + 50));
  v %= 6;

  switch (v) {
    case 0:
      return "\x1b[31;49m";
    case 1:
      return "\x1b[32;49m";
    case 2:
      return "\x1b[33;49m";
    case 3:
      return "\x1b[34;49m";
    case 4:
      return "\x1b[35;49m";
    case 5:
      return "\x1b[36;49m";
    default:
      throw std::runtime_error("???");
  }

  std::unreachable();
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
    thread_fmt = std::format("(thread {})", thread_id);
  else
    thread_fmt = "(hewg)";

  std::scoped_lock lock(stdout_mutex);
  std::cout << std::format(
    "{}{:13}\x1b[39;49m| ", get_color_by_thread_id(), thread_fmt);

  std::cout << s;
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

inline bool
is_subpathed_by(std::filesystem::path const owning_directory,
                std::filesystem::path const child)
{
  if (not std::filesystem::is_directory(owning_directory))
    throw std::runtime_error(
      "is_subpathed_by is given a non-directory as owner");

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
create_directory_checked(std::filesystem::path const what);

std::filesystem::path const&
get_home_directory();
