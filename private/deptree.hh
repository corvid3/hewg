#pragma once

#include "app.hh"
#include "confs.hh"
#include "packages.hh"

#include <memory>

class AppContext;
class PackageContext;

/* FIXME: move deptreectx declarations into the header */
struct DeptreeCtx;
struct DeptreeDeleter {
  void
  operator()(DeptreeCtx*);
};

using Deptree = std::unique_ptr<DeptreeCtx, DeptreeDeleter>;

auto
build_dependency_tree(ConfigurationFile const& config,
                      PackageCacheDB const&    db,
                      TargetTriplet const&     this_target) -> Deptree;

auto
collect_packages_to_include(AppContext const& ctx, PackageContext const& pkg)
  -> std::set<PackageIdentifier>;

auto
collect_packages_to_link(AppContext const& ctx, PackageContext const& pkg)
  -> std::set<PackageIdentifier>;
