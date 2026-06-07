#pragma once

#include "app.hh"
#include "build.hh"
#include "cmdline.hh"
#include "confs.hh"
#include "deptree.hh"
#include "packages.hh"
#include "target.hh"

void
link_executable(AppContext const&,
                PackageContext const& pkg,
                BuildContext const&,
                std::span<std::filesystem::path const> object_files);

void
pack_static_library(AppContext const&,
                    BuildContext const&,
                    std::span<std::filesystem::path const> object_files);

void
shared_link(AppContext const&,
            PackageContext const& pkg,
            BuildContext const&,
            std::span<std::filesystem::path const> object_files);
