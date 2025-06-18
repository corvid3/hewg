#pragma once

#include "cmdline.hh"
#include "confs.hh"
#include "thread_pool.hh"

void
build(ThreadPool& threads,
      ConfigurationFile const& config,
      BuildOptions const& options,
      std::string_view build_profile);
