#pragma once

#include <format>
#include <jayson.hh>
#include <regex>
#include <scl.hh>
#include <string>

#include "cmdline.hh"
#include "confs.hh"

class TargetTriplet
{
public:
  TargetTriplet(std::string_view arch,
                std::string_view os,
                std::string_view vendor);

  // TODO: parse
  TargetTriplet(std::string_view);

  bool operator==(TargetTriplet const& rhs) const
  {
    return this->m_architecture == rhs.m_architecture and
           this->m_os == rhs.m_os and this->m_vendor == rhs.m_vendor;
  }

  std::string to_string() const;

  auto architecture() const { return m_architecture; }
  auto os() const { return m_os; }
  auto vendor() const { return m_vendor; }

private:
  std::string m_architecture;
  std::string m_os;
  std::string m_vendor;

public:
  using jayson_fields = std::tuple<
    jayson::obj_field<"architecture", &TargetTriplet::m_architecture>,
    jayson::obj_field<"os", &TargetTriplet::m_os>,
    jayson::obj_field<"vendor", &TargetTriplet::m_vendor>>;

  static bool constexpr jayson_explicitly_constructible = true;
};

// NOTE: don't directly use scl::deserialize,
// use get_target_file instead
struct TargetFile
{
  TargetFile(TargetTriplet const triplet)
    : triplet(triplet) {};

  TargetTriplet triplet;

  std::string cxx;
  std::string cc;
  std::string ld;
  std::string ar;

  using scl_fields = std::tuple<scl::field<&TargetFile::cxx, "cxx">,
                                scl::field<&TargetFile::cc, "cc">,
                                scl::field<&TargetFile::ld, "ld">,
                                scl::field<&TargetFile::ar, "ar">>;
};

ConfigurationFile
get_config_file(ToplevelOptions const&, std::filesystem::path path);

TargetFile
get_target_file(TargetTriplet const target_name);

#ifdef __linux__
constexpr auto THIS_TARGET = "x86-linux-gnu";
#endif

template<>
struct std::formatter<TargetTriplet> : std::formatter<std::string>
{
  auto format(TargetTriplet const& trip, format_context& ctx) const
  {
    return formatter<string>::format(trip.to_string(), ctx);
  }
};
