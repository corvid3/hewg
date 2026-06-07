#pragma once

/*
  analysis for which files must be rebuilt
*/

#include "packages.hh"
#include "srcrelatives.hh"
#include "target.hh"

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <vector>

enum class FileType : uint8_t {
  CSource,
  CHeader,

  CXXSource,
  CXXHeader,
};

auto
translate_filename_to_filetype(std::filesystem::path const& s) -> FileType;

// identifiers rely on triplet, so you need to pass one
auto
get_this_package_ident(ConfigurationFile const& config, TargetTriplet triplet)
  -> PackageIdentifier;

auto
get_artifact_folder(PackageIdentifier const& ident) -> std::filesystem::path;

auto
get_c_standard_string(int std) -> std::string;
auto
get_cxx_standard_string(int std) -> std::string;

auto
get_cxx_source_filepaths(ConfigurationFile const&)
  -> std::vector<std::filesystem::path>;

auto
get_c_source_filepaths(ConfigurationFile const&)
  -> std::vector<std::filesystem::path>;

auto
static_library_name_for_project(ConfigurationFile const& config, bool PIE)
  -> std::string;
auto
dynamic_library_name_for_project(ConfigurationFile const& config)
  -> std::string;

auto
get_cache_folder(std::string_view target_name, bool release, bool pic)
  -> std::filesystem::path;

auto
object_file_for_cxx(std::filesystem::path const& cache_folder,
                    std::filesystem::path const& src_folder,
                    std::filesystem::path const& abs_src_file)
  -> std::filesystem::path;

auto
object_file_for_c(std::filesystem::path const& cache_folder,
                  std::filesystem::path const& src_folder,
                  std::filesystem::path const& abs_src_file)
  -> std::filesystem::path;

// converts a source-path source file
// to it's cache-path dependency file
auto
depfile_for_cxx(std::filesystem::path const& cache_folder,
                std::filesystem::path const& src_folder,
                std::filesystem::path const& abs_src_file)
  -> std::filesystem::path;

auto
depfile_for_c(std::filesystem::path const& cache_folder,
              std::filesystem::path const& src_folder,
              std::filesystem::path const& abs_src_file)
  -> std::filesystem::path;

// converts the list of source files in the config
// into a list of files for each filetype
auto
get_files_by_type(std::span<std::filesystem::path const> source_files, FileType)
  -> std::vector<std::filesystem::path>;

auto
get_modification_date_of_file(std::filesystem::path const& p)
  -> std::optional<unsigned>;

// sources can be any number of c/cxx files
// returns a sublist of the provided files
// that should be rebuilt, based on
// modification date and include dependencies
auto
mark_c_files_for_rebuild(CSourceRelatives const&)
  -> std::vector<std::reference_wrapper<CSourceRelatives::File const>>;

auto
mark_cxx_files_for_rebuild(CXXSourceRelatives const&)
  -> std::vector<std::reference_wrapper<CXXSourceRelatives::File const>>;
