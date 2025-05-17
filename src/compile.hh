#pragma once

#include "confs.hh"
#include "thread_pool.hh"

/*
  should eventually split it up such that
  each compile step returns a listing of all object files,
  then a final link step
*/

void
compile_c_cxx(ThreadPool& pool, ConfigurationFile const& projconf);
