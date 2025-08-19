#pragma once

#include <expected>
#include <filesystem>
#include <future>

#include "confs.hh"
#include "target.hh"
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

std::pair<std::vector<std::filesystem::path>,
          std::vector<std::future<std::optional<std::string>>>>
compile_cxx(ThreadPool& pool,
            ConfigurationFile const& config,
            TargetFile const& tools,
            std::filesystem::path const& cache_folder,
            bool const release,
            bool const PIC);

std::pair<std::vector<std::filesystem::path>,
          std::vector<std::future<std::optional<std::string>>>>
compile_c(ThreadPool& pool,
          ConfigurationFile const& config,
          TargetFile const& tools,
          std::filesystem::path const& cache_folder,
          bool const release,
          bool const PIC);

// builds the special hewg symbols object file
// and returns a path to it
std::filesystem::path
compile_hewgsym(ConfigurationFile const& config,
                TargetFile const& tools,
                bool PIC);
