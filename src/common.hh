#pragma once

#include "thread_pool.hh"
#include <array>
#include <cmath>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

using namespace std::string_view_literals;

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

  return std::mismatch(owning_directory.begin(),
                       owning_directory.end(),
                       child.begin(),
                       child.end())
           .first == owning_directory.end();
}
