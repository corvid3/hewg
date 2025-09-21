#pragma once

#include <expected>
#include <filesystem>
#include <future>

#include "common.hh"
#include "confs.hh"
#include "packages.hh"
#include "target.hh"
#include "thread_pool.hh"

/*
  should eventually split it up such that
  each compile step returns a listing of all object files,
  then a final link step
*/

/* returns a list of handles to threads created,
 * caller must await these threads
 */
std::vector<std::future<std::optional<std::string>>>
compile_cxx(ThreadPool& pool,
            int const std,
            std::span<std::string const> extra_flags,
            CXXSourceRelatives const& source_relatives,
            TargetFile const& tools,
            PackageIdentifier const& this_package_ident,
            std::span<std::filesystem::path const> include_directories,
            bool const release,
            bool const PIC);

std::vector<std::future<std::optional<std::string>>>
compile_c(ThreadPool& pool,
          int const std,
          std::span<std::string const> extra_flags,
          CSourceRelatives const& source_relatives,
          TargetFile const& tools,
          PackageIdentifier const& this_package_ident,
          std::span<std::filesystem::path const> include_directories,
          bool const release,
          bool const PIC);

// builds the special hewg symbols object file
// and returns a path to it
std::filesystem::path
compile_hewgsym(ConfigurationFile const& config,
                TargetFile const& tools,
                PackageIdentifier const& this_package_ident,
                bool PIC);
