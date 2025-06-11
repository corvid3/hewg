#pragma once

#include <filesystem>
#include <vector>
struct Depfile
{
  std::filesystem::path obj_path;
  std::filesystem::path src_path;

  // includes the source file, as the object file
  // depends on it for rebuilds
  std::vector<std::filesystem::path> dependencies;
};

Depfile
parse_depfile(std::filesystem::path const);
