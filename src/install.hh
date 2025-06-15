#pragma once

#include "confs.hh"
#include <filesystem>

void
install(ConfigurationFile const& config);

std::filesystem::path
get_package_by_name(std::string_view name);
