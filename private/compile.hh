#pragma once

#include "app.hh"
#include "confs.hh"
#include "packages.hh"
#include "srcrelatives.hh"
#include "target.hh"
#include "thread_pool.hh"

#include <filesystem>
#include <future>

class BuildContext;

/*
  should eventually split it up such that
  each compile step returns a listing of all object files,
  then a final link step
*/

/* returns a list of handles to threads created,
 * caller must await these threads
 */
auto
compile_cxx(AppContext const&         ctx,
            BuildContext const&       build_ctx,
            CXXSourceRelatives const& src)
  -> std::vector<std::future<std::optional<std::string>>>;

auto
compile_c(AppContext const&       ctx,
          BuildContext const&     build_ctx,
          CSourceRelatives const& src)
  -> std::vector<std::future<std::optional<std::string>>>;

// builds the special hewg symbols object file
// and returns a path to it
auto
compile_hewgsym(AppContext const&, BuildContext const&)
  -> std::filesystem::path;

auto
generate_c_cxx_file_flags(std::filesystem::path const& filepath,
                          std::filesystem::path const& depfile,
                          std::filesystem::path const& object_file)
  -> std::vector<std::string>;

auto
generate_c_flags(AppContext const&                  ctx,
                 PackageIdentifier const&           ident,
                 bool                               PIC,
                 std::set<PackageIdentifier> const& include_dirs)
  -> std::vector<std::string>;

auto
generate_cxx_flags(AppContext const&                  ctx,
                   PackageIdentifier const&           ident,
                   bool                               PIC,
                   std::set<PackageIdentifier> const& include_dirs)
  -> std::vector<std::string>;
