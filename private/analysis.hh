#pragma once

/*
  analysis for which files must be rebuilt
*/

#include "common.hh"
#include "confs.hh"
#include "paths.hh"
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

enum class FileType
{
  CSource,
  CHeader,

  CXXSource,
  CXXHeader,
};

FileType
translate_filename_to_filetype(std::filesystem::path const s);

std::vector<std::filesystem::path>
get_source_filepaths(ConfigurationFile const&);

// converts a source-path source file
// to it's cache-path object file
std::filesystem::path
object_file_for(std::filesystem::path in);

// converts a source-path source file
// to it's cache-path dependency file
std::filesystem::path
depfile_for(std::filesystem::path in);

// converts the list of source files in the config
// into a list of files for each filetype
std::vector<std::filesystem::path>
get_files_by_type(std::span<std::filesystem::path const> source_files,
                  FileType);
std::optional<unsigned>
get_modification_date_of_file(std::filesystem::path const p);

std::vector<std::filesystem::path>
mark_c_cxx_files_for_rebuild(
  std::span<std::filesystem::path const> source_files);

bool
semantically_valid(version_triplet const request_for,
                   version_triplet const we_have);

std::optional<version_triplet>
select_best_compatable_semver(std::span<version_triplet const> list,
                              version_triplet const requested);
