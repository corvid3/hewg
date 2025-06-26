#include "common.hh"
#include "depfile.hh"
#include "paths.hh"

#include <lexible.hh>
#include <optional>

enum class TokenType
{
  LEXIBLE_EOF,

  Skip,
  Identifier,
  Colon,
  Backslash,
};

constexpr std::string_view skip_regex = R"(\s+)";

// parser doesn't need to be very robust,
// we verify the filepaths are ok afterwards
constexpr std::string_view identifier_regex = R"(([a-zA-Z0-9_\-\.\/]|\\ )+)";
constexpr std::string_view colon_regex = ":";
constexpr std::string_view backslash_regex = R"(\\)";

using skip_morpheme = lexible::morpheme<skip_regex, TokenType::Skip, 0>;
using identifier_morpheme =
  lexible::morpheme<identifier_regex, TokenType::Identifier, 1>;
using colon_morpheme = lexible::morpheme<colon_regex, TokenType::Colon, 1>;
using backslash_morpheme =
  lexible::morpheme<backslash_regex, TokenType::Backslash, 2>;

using lexer = lexible::lexer<TokenType,
                             skip_morpheme,
                             identifier_morpheme,
                             colon_morpheme,
                             backslash_morpheme>;

struct State
{};

using pctx = lexible::ParsingContext<lexer::token, State>;

struct Depany
  : pctx::Any<pctx::MorphemeParser<TokenType::Identifier,
                                   "expected filepath when parsing depfile">,
              pctx::MorphemeParser<TokenType::Backslash, "expected backslash">>
{
  std::optional<std::filesystem::path> operator()(State&,
                                                  std::string_view what,
                                                  pctx::placeholder_t<0>)
  {
    return what;
  }

  std::optional<std::filesystem::path> operator()(State&,
                                                  std::string_view,
                                                  pctx::placeholder_t<1>)
  {
    return std::nullopt;
  }
};

struct Dependencies : pctx::Repeat<Depany, true>
{
  auto operator()(State&,
                  std::span<std::optional<std::filesystem::path>> in) const
  {
    std::vector<std::filesystem::path> out;
    for (auto const& x : in)
      if (x)
        out.push_back(*x);
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

    if (dependencies.size() < 1)
      throw std::runtime_error("depfile has a corrupted dependency list, "
                               "object file does not depend on source file");

    Depfile out;
    out.obj_path = std::filesystem::path(lhs);
    out.src_path = dependencies.front();

    for (auto i = dependencies.begin(); i != dependencies.end(); i++)
      out.dependencies.push_back(std::filesystem::path(*i));

    return out;
  }
};

using parser = pctx::Engine<pctx::ExpectEOF<DepfileParser>>;

Depfile
parse_depfile(std::filesystem::path const path)
{
  // if (not is_subpathed_by(hewg_c_dependency_cache_path, path) and
  //     not is_subpathed_by(hewg_cxx_dependency_cache_path, path))
  //   throw std::runtime_error("parse_depfiles given a path that doesn't point
  //   "
  //                            "into the dependency cache path");

  auto const contents = read_file(path);
  auto const toks = lexer(contents).consume_all();
  auto const out = parser(std::move(toks)).parse();

  if (not out)
    throw std::runtime_error(
      std::format("failed to parse depfile:\n{}", out.error().what()));

  return *out;
}
