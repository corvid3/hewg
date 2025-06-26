#pragma once

/*
  analysis for which files must be rebuilt
*/

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "confs.hh"

enum class FileType
{
  CSource,
  CHeader,

  CXXSource,
  CXXHeader,
};

FileType
translate_filename_to_filetype(std::filesystem::path const s);

std::string
get_c_standard_string(int std);
std::string
get_cxx_standard_string(int std);

std::vector<std::filesystem::path>
get_cxx_source_filepaths(ConfigurationFile const&);

std::vector<std::filesystem::path>
get_c_source_filepaths(ConfigurationFile const&);

std::string
static_library_name_for_project(ConfigurationFile const& config,
                                bool const PIE);
std::string
dynamic_library_name_for_project(ConfigurationFile const& config);

std::filesystem::path
get_target_folder_for_build_profile(std::string_view const profile);

std::filesystem::path
get_cache_folder(std::string_view build_profile, bool release, bool pic);

// converts a source-path source file
// to it's cache-path object file
std::filesystem::path
object_file_for_cxx(std::filesystem::path cache_folder,
                    std::filesystem::path in);

std::filesystem::path
object_file_for_c(std::filesystem::path cache_folder, std::filesystem::path in);

// converts a source-path source file
// to it's cache-path dependency file
std::filesystem::path
depfile_for_cxx(std::filesystem::path cache_folder, std::filesystem::path in);

std::filesystem::path
depfile_for_c(std::filesystem::path cache_folder, std::filesystem::path in);

// converts the list of source files in the config
// into a list of files for each filetype
std::vector<std::filesystem::path>
get_files_by_type(std::span<std::filesystem::path const> source_files,
                  FileType);

std::optional<unsigned>
get_modification_date_of_file(std::filesystem::path const p);

// sources can be any number of c/cxx files
// returns a sublist of the provided files
// that should be rebuilt, based on
// modification date and include dependencies
std::vector<std::filesystem::path>
mark_c_files_for_rebuild(std::filesystem::path cache_folder,
                         std::span<std::filesystem::path const> sources);

std::vector<std::filesystem::path>
mark_cxx_files_for_rebuild(std::filesystem::path cache_folder,
                           std::span<std::filesystem::path const> sources);

bool
semantically_valid(version_triplet const request_for,
                   version_triplet const we_have);

std::optional<version_triplet>
select_best_compatable_semver(std::span<version_triplet const> list,
                              version_triplet const requested);
