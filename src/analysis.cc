#include "analysis.hh"
#include "common.hh"
#include "confs.hh"
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

unsigned
get_modification_date_of_file(std::filesystem::path const p)
{
  struct stat attr;
  stat(p.c_str(), &attr);
  return attr.st_mtime;
}

class ModificationDateAccessor::impl
{
  // defer access from disk
  std::optional<jayson::val> mod_file;

public:
  std::mutex mutex;

  jayson::obj& get_file()
  {
    if (not mod_file) {
      // touch the date cache file
      if (not std::filesystem::exists(hewg_modification_date_cache_path))
        std::ofstream(hewg_modification_date_cache_path) << "{}";

      mod_file =
        jayson::val::parse(read_file(hewg_modification_date_cache_path));
    }

    return mod_file->as_else_throw<jayson::obj>(
      "mod file isn't a json object?");
  }

  ~impl()
  {
    if (mod_file)
      std::ofstream(hewg_modification_date_cache_path) << mod_file->serialize();
  }
};

std::unique_ptr<ModificationDateAccessor::impl>
  ModificationDateAccessor::m_impl{ new impl() };

ModificationDateAccessor::ModificationDateAccessor()
  : m_lock(m_impl->mutex)
{
}

ModificationDateAccessor::~ModificationDateAccessor() = default;

void
ModificationDateAccessor::update_cached_modification_date_for(
  std::filesystem::path const path,
  unsigned const timestamp)
{
  auto& file = m_impl->get_file();

  auto const full_path = std::filesystem::absolute(path).string();

  std::string const timestamp_str = std::to_string(timestamp);

  file.insert_or_assign(path.string(), timestamp_str);
}

std::optional<unsigned>
ModificationDateAccessor::get_cached_modification_date_for(
  std::filesystem::path const path)
{
  auto& file = m_impl->get_file();

  auto const full_path = std::filesystem::absolute(path).string();

  if (not file.contains(full_path))
    return std::nullopt;

  auto const timestamp_str = file.at(full_path).as_else_throw<std::string>(
    "timestamp in modification date cache file isn't a string?");

  unsigned timestamp;
  std::from_chars(&*timestamp_str.begin(), &*timestamp_str.end(), timestamp);

  return timestamp;
}

std::vector<std::filesystem::path>
get_files_newer_than_cached_mod_date(
  std::span<std::filesystem::path const> source_files)
{
  ModificationDateAccessor accessor;

  std::vector<std::filesystem::path> rebuild;

  for (auto const& source : source_files) {
    auto const cached_mod_date =
      accessor.get_cached_modification_date_for(source);

    if (not cached_mod_date)
      rebuild.push_back(source);
    else if (cached_mod_date < get_modification_date_of_file(source))
      rebuild.push_back(source);
  }

  return rebuild;
}

// std::vector<DepNode>
// get_dependencies(std::span<std::filesystem::path const> source_files)
// {
//   for (auto const& source_file : source_files) {
//     auto const depfile = depfile_for(source_file);
//   }
// };

std::vector<std::filesystem::path>
get_cxx_files_to_rebuild(std::span<std::filesystem::path const> files)
{
  auto const cxx_source_files = get_files_by_type(files, FileType::CXXSource);

  // auto const get_all_deps = get_dependency_tree();

  auto const rebuild_by_mod_date =
    get_files_newer_than_cached_mod_date(cxx_source_files);

  // TODO: implement dependency retriggers,
  // that is for every cxx file that depends on another/header file,
  // retrigger

  return rebuild_by_mod_date;
}
