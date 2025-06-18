#pragma once

#include <filesystem>

#include "confs.hh"
#include "thread_pool.hh"

/*
  should eventually split it up such that
  each compile step returns a listing of all object files,
  then a final link step
*/

// returns all of the object files compiled
// handles incremental compilation
// common flags should be a set of flags
// passed to
std::vector<std::filesystem::path>
compile_c_cxx(ThreadPool& threads,
              ConfigurationFile const& config,
              bool const release,
              bool const PIC);
