#pragma once

#include "cmdline.hh"
#include "confs.hh"
#include <filesystem>

void
install(ConfigurationFile const& config,
        InstallOptions const& options,
        std::string_view profile);

std::filesystem::path
get_package_by_name(std::string_view name);
