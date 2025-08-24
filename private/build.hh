#pragma once

#include "cmdline.hh"
#include "confs.hh"
#include "packages.hh"
#include "target.hh"
#include "thread_pool.hh"

void
build(ThreadPool& threads,
      ConfigurationFile const& config,
      PackageCacheDB& db,
      TargetFile const& target,
      BuildOptions const& options);
