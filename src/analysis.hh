#pragma once

/*
  analysis for which files must be rebuilt
*/

#include "common.hh"
#include "confs.hh"
#include "paths.hh"
#include <filesystem>
#include <mutex>
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

unsigned
get_modification_date_of_file(std::filesystem::path const p);

// accessor structure for getting the cached'
// modification dates of files
class ModificationDateAccessor
{
  class impl;
  static std::unique_ptr<impl> m_impl;

  std::scoped_lock<std::mutex> m_lock;

public:
  ModificationDateAccessor();
  ~ModificationDateAccessor();

  void update_cached_modification_date_for(std::filesystem::path,
                                           unsigned timestamp);

  std::optional<unsigned> get_cached_modification_date_for(
    std::filesystem::path);
};

std::vector<std::filesystem::path>
get_cxx_files_to_rebuild(std::span<std::filesystem::path const> source_files);
