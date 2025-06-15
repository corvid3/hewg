#include "analysis.hh"
#include "common.hh"
#include "confs.hh"
#include "depfile.hh"
#include "paths.hh"
#include <algorithm>
#include <filesystem>
#include <format>
#include <iterator>
#include <jayson.hh>
#include <optional>
#include <stdexcept>
#include <sys/stat.h>

FileType
translate_filename_to_filetype(std::filesystem::path const s)
{
  auto const ext = s.extension();

  if (ext == ".c")
    return FileType::CSource;

  if (ext == ".h")
    return FileType::CHeader;

  if (ext == ".cc" or ext == ".cpp")
    return FileType::CXXSource;

  if (ext == ".hh" or ext == ".hpp")
    return FileType::CXXHeader;

  throw std::runtime_error("unknown filetype in sources");
}

// TODO: i can memoize this
std::vector<std::filesystem::path>
get_source_filepaths(ConfigurationFile const& conf)
{
  std::vector<std::filesystem::path> paths;

  for (auto const& file : conf.files.source) {
    auto const filepath = hewg_src_directory_path / file;

    if (not is_subpathed_by(hewg_src_directory_path, filepath))
      throw std::runtime_error(
        std::format("source file <{}> is outside of the source directory!",
                    filepath.string()));

    paths.push_back(filepath);
  }

  return paths;
}

std::filesystem::path
object_file_for(std::filesystem::path in)
{
  if (not is_subpathed_by(hewg_src_directory_path, in))
    throw std::runtime_error(
      std::format("object_file_for given a path that is not "
                  "subpathed by the src directory, owner = <{}>, child = <{}>",
                  hewg_src_directory_path.string(),
                  in.string()));

  auto const relative_to_src =
    std::filesystem::relative(in, hewg_src_directory_path);
  auto new_path = hewg_object_cache_path / relative_to_src;

  return new_path.replace_extension(".o");
}

std::filesystem::path
depfile_for(std::filesystem::path in)
{
  if (not is_subpathed_by(hewg_src_directory_path, in))
    throw std::runtime_error(
      std::format("depfile_for given a path that is not "
                  "subpathed by the src directory, owner = <{}>, child = <{}>",
                  hewg_src_directory_path.string(),
                  in.string()));

  auto const relative_to_src =
    std::filesystem::relative(in, hewg_src_directory_path);
  auto new_path = hewg_dependency_cache_path / relative_to_src;

  return new_path.replace_extension(".d");
}

// TODO: i can memoize this, do that later
std::vector<std::filesystem::path>
get_files_by_type(std::span<std::filesystem::path const> files,
                  FileType const type)
{
  std::vector<std::filesystem::path> selected;

  for (auto const& source_filepath : files) {
    if (not std::filesystem::is_regular_file(source_filepath))
      throw std::runtime_error(std::format(
        "{} is not a file, despite being listed in project configuration",
        source_filepath.string()));

    auto const extension = translate_filename_to_filetype(source_filepath);

    if (extension == type)
      selected.push_back(source_filepath);
  }

  return selected;
}

std::optional<unsigned>
get_modification_date_of_file(std::filesystem::path const p)
{
  if (not std::filesystem::exists(p))
    return std::nullopt;

  struct stat attr;
  stat(p.c_str(), &attr);
  return attr.st_mtime;
}

std::vector<Depfile>
get_dependencies(std::span<std::filesystem::path const> source_files)
{
  std::vector<Depfile> files;

  for (auto const& source_file : source_files) {
    auto const depfile_path = depfile_for(source_file);

    if (not std::filesystem::exists(depfile_path))
      continue;

    files.push_back(parse_depfile(depfile_path));
  }

  return files;
};

std::vector<std::filesystem::path>
mark_c_cxx_files_for_rebuild(std::span<std::filesystem::path const> files)
{
  auto&& cxx_source_files = get_files_by_type(files, FileType::CXXSource);
  auto&& c_source_files = get_files_by_type(files, FileType::CSource);

  std::vector<std::filesystem::path> collect;
  std::ranges::set_union(
    cxx_source_files, c_source_files, std::inserter(collect, collect.end()));

  std::vector<std::filesystem::path> rebuilds;
  std::vector<Depfile> const depfiles = get_dependencies(collect);

  // lazily find
  // those files which don't have a depfile (thus uncompiled)
  // yes this sucks. i fix it later
  for (auto const& file : collect) {
    for (auto const& df : depfiles)
      if (std::filesystem::absolute(file) ==
          std::filesystem::absolute(df.src_path))
        goto after;

    rebuilds.push_back(file);
  after:;
  }

  for (auto const& depfile : depfiles) {
    auto const obj_md = get_modification_date_of_file(depfile.obj_path);

    if (not obj_md) {
      rebuilds.push_back(depfile.src_path);
      continue;
    }

    // std::stringstream fmt;
    // fmt << std::format(
    //   "object: {}, date: {}\n", depfile.obj_path.string(), *obj_md);

    for (auto const& dep : depfile.dependencies) {
      auto const dep_md = get_modification_date_of_file(dep);

      if (not dep_md)
        continue;

      // fmt << std::format("depfile: {}, date: {}\n", dep.string(), *dep_md);

      if (*dep_md > obj_md) {
        rebuilds.push_back(depfile.src_path);
        break;
      }
    }
    // threadsafe_print(fmt.str(), '\n');
  }

  return rebuilds;
}
