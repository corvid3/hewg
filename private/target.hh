#pragma once

#include "cmdline.hh"
#include "confs.hh"

#include <crow.jayson/jayson.hh>
#include <crow.scl/scl.hh>
#include <filesystem>
#include <format>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

class TargetTriplet
{
public:
  struct empty_m {
  };

  TargetTriplet(std::string_view arch,
                std::string_view os,
                std::string_view vendor);

  explicit TargetTriplet(std::string_view);

  /* constructs an empty target-triplet, will fault on invalid values */
  explicit TargetTriplet(empty_m);

  auto
  operator==(TargetTriplet const& rhs) const -> bool
  {
    check_empty();
    rhs.check_empty();
    return this->m_architecture == rhs.m_architecture and this->m_os == rhs.m_os
       and this->m_vendor == rhs.m_vendor;
  }

  [[nodiscard]]
  auto
  to_string() const -> std::string;

  [[nodiscard]]
  auto
  architecture() const -> std::string_view
  {
    check_empty();
    return m_architecture;
  }

  [[nodiscard]]
  auto
  os() const -> std::string_view
  {
    check_empty();
    return m_os;
  }

  [[nodiscard]]
  auto
  vendor() const -> std::string_view
  {
    check_empty();
    return m_vendor;
  }

private:
  std::string m_architecture;
  std::string m_os;
  std::string m_vendor;
  bool        m_empty;

  void
  check_empty() const
  {
    if (m_empty)
      throw std::runtime_error("empty target triplet found");
  }

public:
  using jayson_fields = std::tuple<
    jayson::obj_field<"architecture", &TargetTriplet::m_architecture>,
    jayson::obj_field<"os", &TargetTriplet::m_os>,
    jayson::obj_field<"vendor", &TargetTriplet::m_vendor>>;

  static bool constexpr jayson_explicitly_constructible = true;
};

// NOTE: don't directly use scl::deserialize,
// use get_target_file instead
class TargetFile
{
public:
  struct Profile {
    std::vector<std::string> cflags;
    std::vector<std::string> cxxflags;

    using scl_fields = std::tuple<scl::field<&Profile::cflags, "c">,
                                  scl::field<&Profile::cxxflags, "cxx">>;
  };

  TargetTriplet triplet;
  std::string   cxx;
  std::string   cc;
  std::string   ld;
  std::string   ar;

  /* debug builds. build metadata should not be stripped and
   * debugging information should be enabled for compilers. */
  Profile debug;

  /* builds that are meant to be fast, without caring for executable size */
  Profile release;

  /* builds that are meant to be small, with only few optimizations for speed */
  Profile small;

  /* loads a TargetFile from the user hewg directory */
  static auto
  Load(TargetTriplet const&) -> TargetFile;

  using scl_fields = std::tuple<scl::field<&TargetFile::cxx, "cxx">,
                                scl::field<&TargetFile::cc, "cc">,
                                scl::field<&TargetFile::ld, "ld">,
                                scl::field<&TargetFile::ar, "ar">>;

  using scl_recurse = std::tuple<scl::field<&TargetFile::debug, "debug">,
                                 scl::field<&TargetFile::release, "release">,
                                 scl::field<&TargetFile::small, "small">>;

private:
  /* default initialize triplet w/ some junk so we can use scl */
  TargetFile()
    : triplet(TargetTriplet::empty_m{}) {};
};

auto
get_config_file(ToplevelOptions const&,
                std::optional<std::reference_wrapper<TargetTriplet const>>,
                std::filesystem::path path) -> ConfigurationFile;

#ifdef __linux__
/* by default, in linux builds, prefer the gnu toolchain. */
constexpr auto THIS_TARGET = "x86-linux-gnu";
#endif

template<>
struct std::formatter<TargetTriplet> : std::formatter<std::string> {
  auto
  format(TargetTriplet const& trip, format_context& ctx) const
  {
    return formatter<string>::format(trip.to_string(), ctx);
  }
};
