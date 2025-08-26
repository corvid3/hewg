#include <terse/terse.hh>

#include "cmdline.hh"

decltype(terse::execute<ToplevelOptions>({}, {}))
parse_cmdline(int argc, char** argv)
{
  return terse::execute<ToplevelOptions>(argc, argv);
}
