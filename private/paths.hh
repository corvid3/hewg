#pragma once

#include <filesystem>

#include "common.hh"

auto static const hewg_config_path =
  std::filesystem::current_path() / "hewg.scl";

auto static const hewg_cache_path = std::filesystem::current_path() / ".hcache";

auto const hewg_hook_path = std::filesystem::current_path() / "hooks";
auto const hewg_hook_cache_path = hewg_cache_path / "hooks.json";

auto const hewg_builtinsym_cache_path = hewg_cache_path / "hewgsyms.json";
auto const hewg_builtinsym_src_path = hewg_cache_path / "hewgsyms.c";
auto const hewg_builtinsym_obj_path = hewg_cache_path / "hewgsyms.o";

auto static const hewg_cxx_object_cache_path = hewg_cache_path / "objects";
auto static const hewg_cxx_dependency_cache_path = hewg_cache_path / "deps";
auto static const hewg_c_object_cache_path = hewg_cache_path / "cobjects";
auto static const hewg_c_dependency_cache_path = hewg_cache_path / "cdeps";

auto static const hewg_modification_date_cache_path =
  hewg_cache_path / "modification_dates.json";

auto static const hewg_cxx_src_directory_path =
  std::filesystem::current_path() / "src";

auto static const hewg_c_src_directory_path =
  std::filesystem::current_path() / "csrc";

auto static const hewg_public_header_directory_path =
  std::filesystem::current_path() / "include";
auto static const hewg_private_header_directory_path =
  std::filesystem::current_path() / "private";
auto static const hewg_err_directory_path =
  std::filesystem::current_path() / "err";
auto static const hewg_target_directory_path =
  std::filesystem::current_path() / "target";

auto const user_hewg_directory = get_home_directory() / ".hewg";
auto const hewg_packages_directory = user_hewg_directory / "packages";
auto const hewg_bin_directory = user_hewg_directory / "bin";
