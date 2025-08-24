#pragma once

#include "cmdline.hh"
#include "confs.hh"
#include "packages.hh"
#include "target.hh"

void
install(ConfigurationFile const& config,
        PackageCacheDB& db,
        TargetTriplet target,
        BuildOptions const& options);
