#include "analysis.hh"
#include "srcrelatives.hh"

CSourceRelatives::CSourceRelatives(
  std::filesystem::path const& cache_directory,
  std::filesystem::path const& source_directory,
  std::span<std::filesystem::path const> source_file)
  : m_cacheDirectory(cache_directory)
  , m_sourceDirectory(source_directory)
{
  for (auto const& src : source_file)
    m_files.push_back(
      File(src,
           object_file_for_c(
             cache_directory, source_directory, source_directory / src),
           depfile_for_c(
             cache_directory, source_directory, source_directory / src)));

  m_files.shrink_to_fit();
}

CXXSourceRelatives::CXXSourceRelatives(
  std::filesystem::path const& cache_directory,
  std::filesystem::path const& source_directory,
  std::span<std::filesystem::path const> source_file)
  : m_cacheDirectory(cache_directory)
  , m_sourceDirectory(source_directory)
{
  for (auto const& src : source_file)
    m_files.push_back(
      File(src,
           object_file_for_cxx(
             cache_directory, source_directory, source_directory / src),
           depfile_for_cxx(
             cache_directory, source_directory, source_directory / src)));

  m_files.shrink_to_fit();
}
