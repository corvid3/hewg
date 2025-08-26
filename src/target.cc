#include "packages.hh"
#include "target.hh"

TargetTriplet::TargetTriplet(std::string_view arch,
                             std::string_view os,
                             std::string_view vendor)
  : m_architecture(arch)
  , m_os(os)
  , m_vendor(vendor)
{
  auto const verify = [](std::string_view in) {
    std::regex static const check("^[a-zA-Z0-9]+$");
    if (not std::regex_match(in.begin(), in.end(), check))
      throw std::runtime_error("target triplet may only contain [a-zA-Z0-9]+");
  };

  verify(arch);
  verify(os);
  verify(vendor);
};

TargetTriplet::TargetTriplet(std::string_view in)
{
  std::cmatch matches;

  if (not std::regex_match(in.begin(), in.end(), matches, regexes::target))
    throw std::runtime_error("invalid targettriplet parse");

  m_architecture = matches[1].str();
  m_os = matches[2].str();
  m_vendor = matches[3].str();
}

std::string
TargetTriplet::to_string() const
{
  return std::format("{}-{}-{}", m_architecture, m_os, m_vendor);
}
