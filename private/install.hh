#pragma once

#include "cmdline.hh"
#include "confs.hh"
#include "target.hh"

void
install(ConfigurationFile const& config,
        TargetTriplet target,
        BuildOptions const& options);
