#include "common.hh"
#include "depfile.hh"
#include "paths.hh"

#include <lexible.hh>

enum class TokenType
{
  LEXIBLE_EOF,

  Skip,
  Identifier,
  Colon,
};

constexpr std::string_view skip_regex = R"(\s+)";

// parser doesn't need to be very robust,
// we verify the filepaths are ok afterwards
constexpr std::string_view identifier_regex = R"(([a-zA-Z0-9\.\/]|\\ )+)";
constexpr std::string_view colon_regex = ":";

using skip_morpheme = lexible::morpheme<skip_regex, TokenType::Skip, 0>;
using identifier_morpheme =
  lexible::morpheme<identifier_regex, TokenType::Identifier, 1>;
using colon_morpheme = lexible::morpheme<colon_regex, TokenType::Colon, 1>;

using lexer =
  lexible::lexer<TokenType, skip_morpheme, identifier_morpheme, colon_morpheme>;

struct State
{};

using pctx = lexible::ParsingContext<lexer, State>;

struct Dependencies
  : pctx::Repeat<pctx::MorphemeParser<TokenType::Identifier,
                                      "expected filepath when parsing depfile">,
                 true>
{
  std::vector<std::filesystem::path> operator()(
    State&,
    std::span<std::string_view const> in)
  {
    std::vector<std::filesystem::path> out;
    std::ranges::transform(
      in, std::inserter(out, out.end()), [](std::string_view w) {
        return std::filesystem::path(w);
      });
    return out;
  }
};

struct DepfileParser
  : pctx::AndThen<
      pctx::MorphemeParser<TokenType::Identifier,
                           "lhs of depfile isn't an identifier">,
      pctx::MorphemeParser<TokenType::Colon,
                           "expected colon after depfile recipe rule">,
      Dependencies>
{
  std::size_t static constexpr CUT_AT = 0;
  std::string_view static constexpr CUT_ERROR = "failed to parse depfile";

  Depfile operator()(State&,
                     std::tuple<std::string_view,
                                std::string_view,
                                std::vector<std::filesystem::path>> tup)
  {
    auto const& [lhs, _1, dependencies] = tup;

    Depfile out;
    out.path = std::filesystem::path(lhs);
    for (auto i = ++dependencies.begin(); i != dependencies.end(); i++)
      out.dependencies.push_back(std::filesystem::path(*i));

    return out;
  }
};

using parser = pctx::Engine<pctx::ExpectEOF<DepfileParser>>;

Depfile
parse_depfile(std::filesystem::path const path)
{
  if (not is_subpathed_by(hewg_dependency_cache_path, path))
    throw std::runtime_error("parse_depfiles given a path that doesn't point "
                             "into the dependency cache path");

  auto const contents = read_file(path);
  auto const out = parser(contents).parse();

  if (not out)
    throw std::runtime_error(
      std::format("failed to parse depfile:\n{}", out.error().what()));

  return *out;
}
