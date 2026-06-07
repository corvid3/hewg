#pragma once

#include "app.hh"
#include "build.hh"
#include "cmdline.hh"
#include "confs.hh"
#include "packages.hh"
#include "target.hh"

void
install(AppContext const& ctx, PackageContext const& pkg);

void
select_executable(PackageCacheDB const& db, PackageIdentifier package_ident);
