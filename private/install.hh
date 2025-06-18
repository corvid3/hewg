#pragma once

#include <filesystem>

#include "cmdline.hh"
#include "confs.hh"

void
install(ConfigurationFile const& config,
        InstallOptions const& options,
        std::string_view profile);
