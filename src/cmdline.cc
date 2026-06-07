#include "cmdline.hh"

#include <cassert>
#include <crow.terse/terse.hh>

auto
parse_cmdline(int argc, char** argv)
  -> decltype(terse::execute<ToplevelOptions>({}, {}))
{
  return terse::execute<ToplevelOptions>(argc, argv);
}
