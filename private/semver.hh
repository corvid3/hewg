#pragma once

#include <compare>
#include <jayson/jayson.hh>
#include <optional>
#include <string>
#include <string_view>

class SemVer
{
public:
  // parses a valid semver
  SemVer(int const major,
         int const minor,
         int const patch,
         std::optional<std::string> prerelease,
         std::optional<std::string> metadata)
    : m_major(major)
    , m_minor(minor)
    , m_patch(patch)
    , m_prerelease(prerelease)
    , m_metadata(metadata) {};

  auto major() const { return m_major; }
  auto minor() const { return m_minor; }
  auto patch() const { return m_patch; }
  auto prerelease() const { return m_prerelease; }
  auto metadata() const { return m_metadata; }

  std::strong_ordering operator<=>(SemVer const&) const;
  bool operator==(SemVer const&) const = default;
  bool operator<(SemVer const&) const = default;
  bool operator>(SemVer const&) const = default;

private:
  int m_major;
  int m_minor;
  int m_patch;

  std::optional<std::string> m_prerelease;
  std::optional<std::string> m_metadata;

public:
  using jayson_fields =
    std::tuple<jayson::obj_field<"major", &SemVer::m_major>,
               jayson::obj_field<"minor", &SemVer::m_minor>,
               jayson::obj_field<"patch", &SemVer::m_patch>,
               jayson::obj_field<"prerelease", &SemVer::m_prerelease, false>,
               jayson::obj_field<"metadata", &SemVer::m_metadata, false>>;

  static bool constexpr jayson_explicitly_constructible = true;
};

std::optional<SemVer> parse_semver(std::string_view);

template<>
struct std::formatter<SemVer> : std::formatter<std::string>
{
  auto format(SemVer const& ver, format_context& ctx) const
  {
    std::stringstream out;
    out << std::format("{}.{}.{}", ver.major(), ver.minor(), ver.patch());

    if (auto const& pre = ver.prerelease(); pre)
      out << std::format("-{}", *pre);

    if (auto const& meta = ver.metadata(); meta)
      out << std::format("+{}", *meta);

    return formatter<string>::format(std::move(out).str(), ctx);
  }
};
