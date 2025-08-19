#include <algorithm>
#include <charconv>
#include <compare>
#include <lexible.hh>
#include <numeric>
#include <optional>
#include <ranges>
#include <regex>

#include "common.hh"
#include "semver.hh"

auto constexpr static semver_regex_str =
  R"(^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$)";

static const std::regex semver_regex(semver_regex_str);

std::optional<SemVer>
parse_semver(std::string_view what)
{
  std::cmatch matches;
  if (not std::regex_match(what.begin(), what.end(), matches, semver_regex))
    throw std::runtime_error("invalid semver");

  auto const major_text = matches[1];
  auto const minor_text = matches[2];
  auto const patch_text = matches[3];
  auto const prerelease_text = matches[4];
  auto const metadata_text = matches[5];

  auto const to_int = [](std::string_view what) -> int {
    int m;
    std::from_chars(what.begin(), what.end(), m);
    return m;
  };

  auto const unwrap_str = [](decltype(prerelease_text)
                               const& in) -> std::optional<std::string> {
    if (not in.matched)
      return std::nullopt;
    else
      return std::optional{ in.str() };
  };

  return SemVer(to_int(major_text.str()),
                to_int(minor_text.str()),
                to_int(patch_text.str()),
                unwrap_str(prerelease_text),
                unwrap_str(metadata_text));
};

static bool
num_only(std::string_view const in)
{
  return std::ranges::fold_left(
    in, true, [](bool fold, char const in) { return fold & isdigit(in); });
}

std::strong_ordering
SemVer::operator<=>(SemVer const& rhs) const
{
  if (major() < rhs.major())
    return std::strong_ordering::less;
  if (minor() < rhs.minor())
    return std::strong_ordering::less;
  if (patch() < rhs.patch())
    return std::strong_ordering::less;

  auto const lhs_pre = prerelease();
  auto const rhs_pre = rhs.prerelease();

  auto const lhs_meta = metadata();
  auto const rhs_meta = rhs.metadata();

  // pre-release has less precedence than normal
  if (not lhs_pre and rhs_pre)
    return std::strong_ordering::greater;
  if (lhs_pre and not rhs_pre)
    return std::strong_ordering::less;

  if (lhs_pre and rhs_pre) {
    auto const lhs_segments = split_by_delim(*lhs_pre, '.');
    auto const rhs_segments = split_by_delim(*rhs_pre, '.');

    auto const compare_sections =
      [](std::string_view lhs,
         std::string_view rhs) static -> std::strong_ordering {
      auto const lhs_num_only = num_only(lhs);
      auto const rhs_num_only = num_only(rhs);

      // short circuits for numbers compared with ascii
      if (lhs_num_only and not rhs_num_only)
        return std::strong_ordering::less;
      if (not lhs_num_only and rhs_num_only)
        return std::strong_ordering::greater;

      // compare numerically
      if (lhs_num_only and rhs_num_only) {
        int lhs_val;
        int rhs_val;

        std::from_chars(lhs.begin(), lhs.end(), lhs_val);
        std::from_chars(rhs.begin(), rhs.end(), rhs_val);

        return lhs_val <=> rhs_val;
      }

      // identifiers are compared ascii
      auto const name_min = std::min(lhs.size(), rhs.size());
      for (auto const& i : std::ranges::iota_view(name_min)) {
        auto const lhs_c = lhs[i];
        auto const rhs_c = rhs[i];

        if (lhs_c < rhs_c)
          return std::strong_ordering::less;
        if (lhs_c > rhs_c)
          return std::strong_ordering::greater;
      }

      return std::strong_ordering::equal;
    };

    // if one prerelease has more fields than the other...
    if (lhs_segments.size() != rhs_segments.size()) {
      // get the end of the shortest prerelease
      auto const end = lhs_segments.size() < rhs_segments.size()
                         ? lhs_segments.end()
                         : rhs_segments.end();

      // try to find the first mismatch,
      // if its at the end of the shortest prerelease, then the
      // longer prerelease is greater.
      if (std::ranges::mismatch(lhs_segments, rhs_segments).in1 == end)
        return lhs_segments.size() < rhs_segments.size()
                 ? std::strong_ordering::less
                 : std::strong_ordering::greater;

      for (auto const i : std::ranges::iota_view(
             std::min(lhs_segments.size(), rhs_segments.size()))) {
        auto const lhs_segment = lhs_segments[i];
        auto const rhs_segment = rhs_segments[i];

        auto const comparison = compare_sections(lhs_segment, rhs_segment);
        if (comparison == std::strong_ordering::less or
            comparison == std::strong_ordering::greater)
          return comparison;
      }

    } else {
      for (auto const i : std::ranges::iota_view(lhs_segments.size())) {
        auto const lhs_segment = lhs_segments[i];
        auto const rhs_segment = rhs_segments[i];

        auto const comparison = compare_sections(lhs_segment, rhs_segment);
        if (comparison == std::strong_ordering::less or
            comparison == std::strong_ordering::greater)
          return comparison;
      }
    }
  }

  // disregard metadata

  return std::strong_ordering::equivalent;
}
