#pragma once

#include "confs.hh"
#include "packages.hh"
#include <memory>

struct DeptreeCtx;
struct DeptreeDeleter
{
  void operator()(DeptreeCtx*);
};

using Deptree = std::unique_ptr<DeptreeCtx, DeptreeDeleter>;

Deptree
build_dependency_tree(ConfigurationFile const& config,
                      PackageCacheDB const& db,
                      TargetTriplet const& this_target);

std::set<PackageIdentifier>
collect_packages_to_include(ConfigurationFile const& config,
                            PackageCacheDB const& db,
                            TargetTriplet const& this_target,
                            Deptree const&);

std::set<PackageIdentifier>
collect_packages_to_link(ConfigurationFile const& config,
                         PackageCacheDB const& db,
                         TargetTriplet const& this_target,
                         Deptree const&);
