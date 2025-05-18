#include "analysis.hh"
#include "common.hh"
#include "confs.hh"
#include "depfile.hh"
#include "paths.hh"
#include <filesystem>
#include <format>
#include <jayson.hh>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
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
    files.push_back(parse_depfile(depfile_path));
  }

  return files;
};

std::vector<std::filesystem::path>
get_cxx_files_to_rebuild(std::span<std::filesystem::path const> files)
{
  auto const cxx_source_files = get_files_by_type(files, FileType::CXXSource);

  // auto const deps = get_dependencies(cxx_source_files);

  // for (auto const& dep : deps) {
  //   std::string msg;
  //   msg.append(std::format("DEP {}\n", dep.path.string()));
  //   for (auto const& d : dep.dependencies)
  //     msg.append(std::format("  {}\n", d.string()));
  //   threadsafe_print(msg, '\n');
  // }

  // TODO: implement dependency retriggers,
  // that is for every cxx file that depends on another/header file,
  // retrigger

  std::vector<std::filesystem::path> rebuilds;

  for (auto const& sf : cxx_source_files) {
    auto const obj = object_file_for(sf);

    auto const obj_md = get_modification_date_of_file(obj);
    auto const sf_md = *get_modification_date_of_file(sf);

    if (!obj_md)
      rebuilds.push_back(sf);
    else if (sf_md >= *obj_md)
      rebuilds.push_back(sf);
  }

  return rebuilds;
}
