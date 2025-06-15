#pragma once

#include <filesystem>

auto static const hewg_config_path =
  std::filesystem::current_path() / "hewg.scl";

auto static const hewg_cache_path = std::filesystem::current_path() / ".hcache";

auto const hewg_hook_path = std::filesystem::current_path() / "hooks";
auto const hewg_hook_cache_path = hewg_cache_path / "hooks.json";

auto const hewg_builtinsym_cache_path = hewg_cache_path / "hewgsyms.json";
auto const hewg_builtinsym_src_path = hewg_cache_path / "hewgsyms.c";
auto const hewg_builtinsym_obj_path = hewg_cache_path / "hewgsyms.o";

auto static const hewg_object_cache_path = hewg_cache_path / "objects";
auto static const hewg_dependency_cache_path = hewg_cache_path / "deps";
auto static const hewg_modification_date_cache_path =
  hewg_cache_path / "modification_dates.json";

auto static const hewg_src_directory_path =
  std::filesystem::current_path() / "src";

auto static const hewg_err_directory_path =
  std::filesystem::current_path() / "err";

auto static const hewg_target_directory_path =
  std::filesystem::current_path() / "target";
