#pragma once

#include "cmdline.hh"
#include "confs.hh"
#include "thread_pool.hh"
#include <filesystem>
#include <span>

/*
  should eventually split it up such that
  each compile step returns a listing of all object files,
  then a final link step
*/

// returns all of the object files
// that should be added to the link procedure
std::vector<std::filesystem::path>
compile_c_cxx(ThreadPool& pool,
              ConfigurationFile const& projconf,
              BuildOptions const& options);

void
link(ConfigurationFile const& config,
     std::span<std::filesystem::path const> object_files,
     std::filesystem::path output_directory);
