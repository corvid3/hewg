#include "packages.hh"
#include "paths.hh"
#include "target.hh"

#include <format>
#include <stdexcept>

TargetTriplet::TargetTriplet(std::string_view arch,
                             std::string_view os,
                             std::string_view vendor)
  : m_architecture(arch)
  , m_os(os)
  , m_vendor(vendor)
  , m_empty(false)
{
  auto const verify = [](std::string_view in) {
    static std::regex const check("^[a-zA-Z0-9]+$");
    if (not std::regex_match(in.begin(), in.end(), check))
      throw std::runtime_error("target triplet may only contain [a-zA-Z0-9]+");
  };

  verify(arch);
  verify(os);
  verify(vendor);
};

TargetTriplet::TargetTriplet(empty_m /*unused*/)
  : m_empty(true) {};

TargetTriplet::TargetTriplet(std::string_view in)
  : m_empty(false)
{
  std::cmatch matches;

  if (not std::regex_match(in.begin(), in.end(), matches, regexes::target))
    throw std::runtime_error("invalid targettriplet parse");

  m_architecture = matches[1].str();
  m_os           = matches[2].str();
  m_vendor       = matches[3].str();
}

auto
TargetTriplet::to_string() const -> std::string
{
  check_empty();
  return std::format("{}-{}-{}", m_architecture, m_os, m_vendor);
}

auto
TargetFile::Load(TargetTriplet const& target) -> TargetFile
{
  static auto target_dir = user_hewg_directory / "targets";
  auto        target_str = target.to_string();
  auto const  filepath   = target_dir / target_str;

  if (not std::filesystem::exists(filepath)) {
    throw std::runtime_error(
      std::format("requested target triplet <{}> tool file doesn't exist",
                  target.to_string()));
  }

  TargetFile into;
  auto       filedata = read_file(filepath);
  scl::file  file(filedata);
  into.triplet = target;

  if (not scl::deserialize(into, file, "tools")) {
    throw std::runtime_error(
      std::format("target file {} is semantically incorrect", target_str));
  }

  return into;
}
