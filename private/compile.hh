#pragma once

#include <expected>
#include <filesystem>
#include <future>

#include "common.hh"
#include "confs.hh"
#include "packages.hh"
#include "srcrelatives.hh"
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
            std::span<std::string const> flags,
            CXXSourceRelatives const& source_relatives,
            TargetFile const& tools);

std::vector<std::future<std::optional<std::string>>>
compile_c(ThreadPool& pool,
          std::span<std::string const> flags,
          CSourceRelatives const& source_relatives,
          TargetFile const& tools);

// builds the special hewg symbols object file
// and returns a path to it
std::filesystem::path
compile_hewgsym(ConfigurationFile const& config,
                TargetFile const& tools,
                PackageIdentifier const& this_package_ident,
                bool PIC);

std::vector<std::string>
generate_c_cxx_file_flags(std::filesystem::path const filepath,
                          std::filesystem::path const depfile,
                          std::filesystem::path const object_file);

std::vector<std::string>
generate_c_flags(std::span<std::string const> user_flags,
                 std::span<std::filesystem::path const> include_dirs,
                 int const std,
                 PackageIdentifier const& ident,
                 bool const is_release,
                 bool const PIC);

std::vector<std::string>
generate_cxx_flags(std::span<std::string const> user_flags,
                   std::span<std::filesystem::path const> include_dirs,
                   int const std,
                   PackageIdentifier const& ident,
                   bool const is_release,
                   bool const PIC);
