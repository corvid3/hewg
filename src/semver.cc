#include <charconv>
#include <compare>
#include <lexible.hh>
#include <optional>
#include <regex>

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

    auto const num_only = [](auto const& in) {
      return std::ranges::fold_left(
        in, true, [](bool fold, char const in) { return fold & isdigit(in); });
    };

    if (num_only(*lhs_pre) and num_only(*rhs_pre)) {
    }
  }

  return std::strong_ordering::equivalent;
}
