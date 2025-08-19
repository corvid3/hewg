#pragma once

#include "cmdline.hh"
#include "confs.hh"
#include "target.hh"
#include "thread_pool.hh"

void
build(ThreadPool& threads,
      ConfigurationFile const& config,
      TargetFile const& target,
      BuildOptions const& options);
