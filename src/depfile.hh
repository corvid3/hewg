#pragma once

#include <filesystem>
#include <vector>
struct Depfile
{
  std::filesystem::path path;
  std::vector<std::filesystem::path> dependencies;
};

Depfile
parse_depfile(std::filesystem::path const);
