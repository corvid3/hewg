#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <jayson/jayson.hh>
#include <optional>
#include <stdexcept>
#include <sys/stat.h>
#include <system_error>
#include <vector>

#include "analysis.hh"
#include "common.hh"
#include "confs.hh"
#include "depfile.hh"
#include "packages.hh"
#include "paths.hh"
#include "semver.hh"

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

PackageIdentifier
get_this_package_ident(ConfigurationFile const& config, TargetTriplet triplet)
{
  auto const version = parse_semver(config.project.version);
  if (not version)
    throw std::runtime_error("invalid project semver while attempting to "
                             "create this packages identifier");

  return PackageIdentifier(
    config.project.org, config.project.name, *version, triplet);
}

std::filesystem::path
get_artifact_folder(PackageIdentifier const& ident)
{
  return hewg_target_directory_path / ident.target().to_string();
}

std::string
get_c_standard_string(int std)
{
  switch (std) {
    case 99:
      return "c99";
    case 11:
      return "c11";
    case 17:
      return "c17";
    case 23:
      return "c23";

    default:
      throw std::runtime_error(std::format("unrecognized std <{}> for C", std));
  }
}

std::string
get_cxx_standard_string(int std)
{
  switch (std) {
    case 98:
      return "c++98";
    case 3:
      return "c++03";
    case 11:
      return "c++11";
    case 14:
      return "c++14";
    case 17:
      return "c++17";
    case 20:
      return "c++20";
    case 23:
      return "c++23";

    default:
      throw std::runtime_error(
        std::format("unrecognized std <{}> for C++", std));
  }
}

std::vector<std::filesystem::path>
get_cxx_source_filepaths(ConfigurationFile const& conf)
{
  std::vector<std::filesystem::path> paths;

  for (auto const& file : conf.cxx.sources) {
    auto const filepath = hewg_cxx_src_directory_path / file;

    if (not is_subpathed_by(hewg_cxx_src_directory_path, filepath))
      throw std::runtime_error(
        std::format("source file <{}> is outside of the source directory!",
                    filepath.string()));

    paths.push_back(filepath);
  }

  return paths;
}

std::vector<std::filesystem::path>
get_c_source_filepaths(ConfigurationFile const& conf)
{
  std::vector<std::filesystem::path> paths;

  for (auto const& file : conf.c.sources) {
    auto const filepath = hewg_c_src_directory_path / file;

    if (not is_subpathed_by(hewg_c_src_directory_path, filepath))
      throw std::runtime_error(
        std::format("source file <{}> is outside of the source directory!",
                    filepath.string()));

    paths.push_back(filepath);
  }

  return paths;
}

std::string
static_library_name_for_project(ConfigurationFile const& config, bool const PIE)
{
  return std::format("lib{}{}.a", config.project.name, PIE ? "-PIE" : "");
}

std::string
dynamic_library_name_for_project(ConfigurationFile const& config)
{
  return std::format("lib{}.so", config.project.name);
}

std::filesystem::path
get_target_folder_for_build_profile(std::string_view const profile)
{
  return hewg_target_directory_path / profile;
}

std::filesystem::path
get_cache_folder(std::string_view target_name, bool release, bool pic)
{
  auto const inner = std::format(
    "{}{}{}", target_name, pic ? "-pic" : "", release ? "-rel" : "");

  auto const folder = hewg_cache_path / "incremental" / inner;

  create_directory_checked(folder);

  if (not is_subpathed_by(hewg_cache_path, folder))
    throw std::runtime_error(
      std::format("cache folder <{}> is not subpathed by <{}>????",
                  hewg_cache_path.string(),
                  folder.string()));

  create_directory_checked(folder / "cxx_objects");
  create_directory_checked(folder / "c_objects");
  create_directory_checked(folder / "cxx_depends");
  create_directory_checked(folder / "c_depends");

  return hewg_cache_path / "incremental" / inner;
}

std::filesystem::path
object_file_for_cxx(std::filesystem::path cache_folder,
                    std::filesystem::path in)
{
  if (not is_subpathed_by(hewg_cxx_src_directory_path, in))
    throw std::runtime_error(
      std::format("object_file_for given a path that is not "
                  "subpathed by the src directory, owner = <{}>, child = <{}>",
                  hewg_cxx_src_directory_path.string(),
                  in.string()));

  auto const relative_to_src =
    std::filesystem::relative(in, hewg_cxx_src_directory_path);

  return (cache_folder / "cxx_objects" / relative_to_src)
    .replace_extension(".o");
}

std::filesystem::path
object_file_for_c(std::filesystem::path cache_folder, std::filesystem::path in)
{
  if (not is_subpathed_by(hewg_c_src_directory_path, in))
    throw std::runtime_error(
      std::format("object_file_for given a path that is not "
                  "subpathed by the src directory, owner = <{}>, child = <{}>",
                  hewg_c_src_directory_path.string(),
                  in.string()));

  auto const relative_to_src =
    std::filesystem::relative(in, hewg_c_src_directory_path);

  return (cache_folder / "c_objects" / relative_to_src).replace_extension(".o");
}

std::filesystem::path
depfile_for_cxx(std::filesystem::path cache_folder, std::filesystem::path in)
{
  if (not is_subpathed_by(hewg_cxx_src_directory_path, in))
    throw std::runtime_error(
      std::format("depfile_for given a path that is not "
                  "subpathed by the src directory, owner = <{}>, child = <{}>",
                  hewg_cxx_src_directory_path.string(),
                  in.string()));

  auto const relative_to_src =
    std::filesystem::relative(in, hewg_cxx_src_directory_path);

  return (cache_folder / "cxx_depends" / relative_to_src)
    .replace_extension(".d");
}

std::filesystem::path
depfile_for_c(std::filesystem::path cache_folder, std::filesystem::path in)
{
  if (not is_subpathed_by(hewg_c_src_directory_path, in))
    throw std::runtime_error(
      std::format("depfile_for given a path that is not "
                  "subpathed by the src directory, owner = <{}>, child = <{}>",
                  hewg_c_src_directory_path.string(),
                  in.string()));

  auto const relative_to_src =
    std::filesystem::relative(in, hewg_c_src_directory_path);

  return (cache_folder / "c_depends" / relative_to_src).replace_extension(".d");
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
  std::error_code code;
  auto const write_time = std::filesystem::last_write_time(p, code);

  if (code)
    return std::nullopt;

  using namespace std::chrono;

  // return time in seconds
  // wow this is ugly
  return duration_cast<seconds>(
           utc_clock::from_sys(file_clock::to_sys(write_time))
             .time_since_epoch())
    .count();
}

std::vector<Depfile> static get_dependencies_for_c(
  std::filesystem::path const cache_folder,
  std::span<std::filesystem::path const> source_files)
{
  std::vector<Depfile> files;

  for (auto const& source_file : source_files) {
    auto const depfile_path = depfile_for_c(cache_folder, source_file);

    if (not std::filesystem::exists(depfile_path))
      continue;

    files.push_back(parse_depfile(depfile_path));
  }

  return files;
};

std::vector<Depfile> static get_dependencies_for_cxx(
  std::filesystem::path const cache_folder,
  std::span<std::filesystem::path const> source_files)
{
  std::vector<Depfile> files;

  for (auto const& source_file : source_files) {
    auto const depfile_path = depfile_for_cxx(cache_folder, source_file);

    if (not std::filesystem::exists(depfile_path))
      continue;

    files.push_back(parse_depfile(depfile_path));
  }

  return files;
};

static std::vector<std::filesystem::path>
mark_c_cxx_files_for_rebuild(std::span<std::filesystem::path const> sources,
                             std::span<Depfile const> depfiles)
{
  std::vector<std::filesystem::path> rebuilds;

  // lazily find
  // those files which don't have a depfile (thus uncompiled)
  // yes this sucks. i fix it later
  for (auto const& file : sources) {
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

    for (auto const& dep : depfile.dependencies) {
      auto const dep_md = get_modification_date_of_file(dep);

      if (not dep_md)
        continue;

      if (*dep_md > obj_md) {
        rebuilds.push_back(depfile.src_path);
        break;
      }
    }
  }

  return rebuilds;
}

std::vector<std::filesystem::path>
mark_c_files_for_rebuild(std::filesystem::path const cache_folder,
                         std::span<std::filesystem::path const> sources)
{
  std::vector<Depfile> const depfiles =
    get_dependencies_for_c(cache_folder, sources);
  return mark_c_cxx_files_for_rebuild(sources, depfiles);
}

std::vector<std::filesystem::path>
mark_cxx_files_for_rebuild(std::filesystem::path const cache_folder,
                           std::span<std::filesystem::path const> sources)
{
  std::vector<Depfile> const depfiles =
    get_dependencies_for_cxx(cache_folder, sources);
  return mark_c_cxx_files_for_rebuild(sources, depfiles);
}

std::optional<version_triplet>
select_best_compatable_semver(std::span<version_triplet const> list_,
                              version_triplet const requested)
{
  std::vector<version_triplet> list(list_.begin(), list_.end());

  auto const [rx, ry, rz] = requested;

  // requested major version must be
  // exactly equal to the provided minor version
  std::erase_if(list, [=](version_triplet const in) {
    auto const [x, _y, _z] = in;
    return x != rx;
  });

  if (list.empty())
    return std::nullopt;

  // requested minor version must be less than or equal to
  // the provided minor version
  std::erase_if(list, [=](version_triplet const in) {
    auto const [_x, y, _z] = in;
    return y < ry;
  });

  if (list.empty())
    return std::nullopt;

  // select the highest patch version
  std::sort(list.begin(), list.end());

  return list.back();
}
