#pragma once

#include <filesystem>
#include <vector>

class CSourceRelatives
{
public:
  struct File
  {
    File(std::filesystem::path source,
         std::filesystem::path object,
         std::filesystem::path depends)
      : source(std::move(source))
      , object(std::move(object))
      , depends(std::move(depends))
    {
    }

    /* source file, relative to source directory */
    std::filesystem::path source;

    /* object file, relative to cache directory */
    std::filesystem::path object;

    /* depends file, relative to cache directory */
    std::filesystem::path depends;
  };

  /* source file must be relative to source_directory */
  CSourceRelatives(std::filesystem::path const& cache_directory,
                   std::filesystem::path const& source_directory,
                   std::span<std::filesystem::path const> source_file);

  CSourceRelatives(const CSourceRelatives&) = delete;
  CSourceRelatives& operator=(const CSourceRelatives&) = delete;

  CSourceRelatives(CSourceRelatives&&) = default;
  CSourceRelatives& operator=(CSourceRelatives&&) = default;

  auto const& cache_directory() const { return m_cacheDirectory; }
  auto const& source_directory() const { return m_sourceDirectory; }

  std::span<File const> files() const { return m_files; }

private:
  std::filesystem::path m_cacheDirectory;
  std::filesystem::path m_sourceDirectory;
  std::vector<File> m_files;
};

class CXXSourceRelatives
{
public:
  struct File
  {
    File(std::filesystem::path source,
         std::filesystem::path object,
         std::filesystem::path depends)
      : source(std::move(source))
      , object(std::move(object))
      , depends(std::move(depends))
    {
    }

    /* source file, relative to source directory */
    std::filesystem::path source;

    /* object file, relative to cache directory */
    std::filesystem::path object;

    /* depends file, relative to cache directory */
    std::filesystem::path depends;
  };

  /* source file must be relative to source_directory */
  CXXSourceRelatives(std::filesystem::path const& cache_directory,
                     std::filesystem::path const& source_directory,
                     std::span<std::filesystem::path const> source_file);

  CXXSourceRelatives(const CXXSourceRelatives&) = delete;
  CXXSourceRelatives& operator=(const CXXSourceRelatives&) = delete;

  CXXSourceRelatives(CXXSourceRelatives&&) = default;
  CXXSourceRelatives& operator=(CXXSourceRelatives&&) = default;

  auto const& cache_directory() const { return m_cacheDirectory; }
  auto const& source_directory() const { return m_sourceDirectory; }

  std::span<File const> files() const { return m_files; }

private:
  std::filesystem::path m_cacheDirectory;
  std::filesystem::path m_sourceDirectory;
  std::vector<File> m_files;
};
