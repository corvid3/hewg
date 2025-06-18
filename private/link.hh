#pragma once

#include "cmdline.hh"
#include "confs.hh"

void
link_executable(ConfigurationFile const& config,
                BuildOptions const& options,
                std::span<std::filesystem::path const> object_files,
                std::filesystem::path output_directory);

void
pack_static_library(ConfigurationFile const& config,
                    std::span<std::filesystem::path const> object_files,
                    std::filesystem::path output_directory,
                    bool const PIC);

void
shared_link(ConfigurationFile const& config,
            BuildOptions const& options,
            std::span<std::filesystem::path const> object_files,
            std::filesystem::path output_directory);
