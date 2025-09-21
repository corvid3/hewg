#include <crow.datalogpp/datalogpp.hh>

#include "analysis.hh"
#include "deptree.hh"

using namespace datalogpp;

struct DeptreeCtx
{
  /*
    Package(ORG, NAME, VERSION, PACKAGE_TYPE, EXTERNAL_INTERNAL).
    Edge(NAME_PARENT, VERSION_PARENT, NAME_CHILD, VERSION_CHILD).
  */

  Interpreter interpreter;
  Predicate &package, &dependency, &dependency_path,
    &repeat_dependency_siblings, &static_to_static_deps, &exists_extern,
    &dependency_cycle, &extern_chain;

  /*
    repeat_dependency_siblings
      * for when a package X appears next to eachother in a graph
  */

  DeptreeCtx()
    : interpreter()
    , package(interpreter.predicate(
        "Package",
        std::array{ "ORG", "NAME", "VERSION", "TARGET", "TYPE" }.size()))
    , dependency(interpreter.predicate("Dependency",
                                       std::array{ "P_ORG",
                                                   "P_NAME",
                                                   "P_VERSION",
                                                   "P_TARGET",
                                                   "C_ORG",
                                                   "C_NAME",
                                                   "C_VERSION",
                                                   "C_TARGET",
                                                   "DEPTYPE",
                                                   "EXT" }
                                         .size()))
    , dependency_path(interpreter.predicate("DependencyPath", 8))
    , repeat_dependency_siblings(
        interpreter.predicate("RepeatDependencySiblings", 0))
    , static_to_static_deps(
        interpreter.predicate("StaticToStaticDependencies", 0))
    , exists_extern(interpreter.predicate("ExistsExternalDependency", 0))
    , dependency_cycle(interpreter.predicate("DependencyCycle", 0))
    , extern_chain(interpreter.predicate("ExternalDependencyChain", 8))
  {
    using namespace datalogpp;

    repeat_dependency_siblings() = "Dependency"_p("ORG"_V,
                                                  "NAME"_V,
                                                  "VERSION"_V,
                                                  "TARGET"_V,
                                                  "C_ORG"_V,
                                                  "C_NAME"_V,
                                                  "C_VERSION"_V,
                                                  "C_TARGET"_V,
                                                  "_"_V,
                                                  "_"_V) +
                                   "Dependency"_p("ORG"_V,
                                                  "NAME"_V,
                                                  "VERSION"_V,
                                                  "TARGET"_V,
                                                  "C_ORG"_V,
                                                  "C_NAME"_V,
                                                  "D_VERSION"_V,
                                                  "D_TARGET"_V,
                                                  "_"_V,
                                                  "_"_V) +
                                   Inequality("D_VERSION"_V, "C_VERSION"_V) +
                                   Inequality("D_TARGET"_V, "C_TARGET"_V);

    static_to_static_deps() =
      "Package"_p("ORG"_V, "NAME"_V, "VERSION"_V, "TARGET"_V, "static") +
      "Dependency"_p("ORG"_V,
                     "NAME"_V,
                     "VERSION"_V,
                     "TARGET"_V,
                     "C_ORG"_V,
                     "C_NAME"_V,
                     "C_VERSION"_V,
                     "C_TARGET"_V,
                     "_"_V,
                     "_"_V) +
      "Package"_p("C_ORG"_V, "C_NAME"_V, "C_VERSION"_V, "C_TARGET"_V, "static");

    exists_extern() = "Dependency"_p("_"_V,
                                     "_"_V,
                                     "_"_V,
                                     "_"_V,
                                     "_"_V,
                                     "_"_V,
                                     "_"_V,
                                     "_"_V,
                                     "_"_V,
                                     "external");

    dependency_path("P_ORG"_V,
                    "P_NAME"_V,
                    "P_VERSION"_V,
                    "P_TARGET"_V,
                    "C_ORG"_V,
                    "C_NAME"_V,
                    "C_VERSION"_V,
                    "C_TARGET"_V) = "Dependency"_p("P_ORG"_V,
                                                   "P_NAME"_V,
                                                   "P_VERSION"_V,
                                                   "P_TARGET"_V,
                                                   "C_ORG"_V,
                                                   "C_NAME"_V,
                                                   "C_VERSION"_V,
                                                   "C_TARGET"_V,
                                                   "_"_V,
                                                   "_"_V);

    dependency_path("P_ORG"_V,
                    "P_NAME"_V,
                    "P_VERSION"_V,
                    "P_TARGET"_V,
                    "C_ORG"_V,
                    "C_NAME"_V,
                    "C_VERSION"_V,
                    "C_TARGET"_V) = "Dependency"_p("P_ORG"_V,
                                                   "P_NAME"_V,
                                                   "P_VERSION"_V,
                                                   "P_TARGET"_V,
                                                   "I_ORG"_V,
                                                   "I_NAME"_V,
                                                   "I_VERSION"_V,
                                                   "I_TARGET"_V,
                                                   "_"_V,
                                                   "_"_V) +
                                    "Dependency"_p("I_ORG"_V,
                                                   "I_NAME"_V,
                                                   "I_VERSION"_V,
                                                   "I_TARGET"_V,
                                                   "C_ORG"_V,
                                                   "C_NAME"_V,
                                                   "C_VERSION"_V,
                                                   "C_TARGET"_V,
                                                   "_"_V,
                                                   "_"_V);

    dependency_cycle() = "DependencyPath"_p(
      "ORG"_V, "NAME"_V, "_"_V, "_"_V, "ORG"_V, "NAME"_V, "_"_V, "_"_V);

    extern_chain("P_ORG"_V,
                 "P_NAME"_V,
                 "P_VERSION"_V,
                 "P_TARGET"_V,
                 "C_ORG"_V,
                 "C_NAME"_V,
                 "C_VERSION"_V,
                 "C_TARGET"_V) = "Dependency"_p("P_ORG"_V,
                                                "P_NAME"_V,
                                                "P_VERSION"_V,
                                                "P_TARGET"_V,
                                                "C_ORG"_V,
                                                "C_NAME"_V,
                                                "C_VERSION"_V,
                                                "C_TARGET"_V,
                                                "_"_V,
                                                "external");

    extern_chain("P_ORG"_V,
                 "P_NAME"_V,
                 "P_VERSION"_V,
                 "P_TARGET"_V,
                 "C_ORG"_V,
                 "C_NAME"_V,
                 "C_VERSION"_V,
                 "C_TARGET"_V) = "Dependency"_p("P_ORG"_V,
                                                "P_NAME"_V,
                                                "P_VERSION"_V,
                                                "P_TARGET"_V,
                                                "I_ORG"_V,
                                                "I_NAME"_V,
                                                "I_VERSION"_V,
                                                "I_TARGET"_V,
                                                "_"_V,
                                                "external"_V) +
                                 "Dependency"_p("I_ORG"_V,
                                                "I_NAME"_V,
                                                "I_VERSION"_V,
                                                "I_TARGET"_V,
                                                "C_ORG"_V,
                                                "C_NAME"_V,
                                                "C_VERSION"_V,
                                                "C_TARGET"_V,
                                                "_"_V,
                                                "external"_V);

    // package("crow", "scl", "0.3.0", "x86-linux-gnu", "static") = {};
    // package("crow", "lexible", "0.4.0", "x86-linux-gnu", "static") = {};
    // package("crow", "bar", "0.2.0", "x86-linux-gnu", "static") = {};

    // dependency("crow",
    //            "scl",
    //            "0.3.0",
    //            "x86-linux-gnu",
    //            "crow",
    //            "lexible",
    //            "0.4.0",
    //            "x86-linux-gnu",
    //            ">",
    //            "external") = {};

    // dependency("crow",
    //            "lexible",
    //            "0.4.0",
    //            "x86-linux-gnu",
    //            "crow",
    //            "bar",
    //            "0.2.0",
    //            "x86-linux-gnu",
    //            ">",
    //            "external") = {};

    interpreter.infer();
  };

  void add_package(std::string_view org,
                   std::string_view name,
                   std::string_view version,
                   std::string_view target,
                   PackageType const package_type)
  {
    package(std::string(org),
            std::string(name),
            std::string(version),
            std::string(target),
            std::string(project_type_to_string(package_type))) = {};
  }

  void add_package(PackageIdentifier const& ident,
                   PackageType const package_type)
  {
    add_package(ident.org(),
                ident.name(),
                std::format("{}", ident.version()),
                std::format("{}", ident.target()),
                package_type);
  }

  void add_dependency(PackageIdentifier const& parent,
                      DependencyIdentifier const& child,
                      bool const is_external)
  {
    dependency(std::string(parent.org()),
               std::string(parent.name()),
               std::format("{}", parent.version()),
               std::format("{}", parent.target()),
               std::string(child.packageIdentifier().org()),
               std::string(child.packageIdentifier().name()),
               std::format("{}", child.packageIdentifier().version()),
               std::format("{}", child.packageIdentifier().target()),
               std::string(sort_to_string(child.sort())),
               std::string(is_external ? "external" : "internal")) = {};
  }
};

void
DeptreeDeleter::operator()(DeptreeCtx* ctx)
{
  delete ctx;
}

static void
add_to_dependency_tree(DeptreeCtx* ctx,
                       PackageCacheDB const& db,
                       std::set<PackageIdentifier>& visited_packages,
                       PackageIdentifier const& identifier)
{
  if (visited_packages.contains(identifier))
    return;

  visited_packages.insert(identifier);

  if (not db.contains(identifier)) {
    throw std::runtime_error(
      std::format("package {} does not exist", identifier));
  }

  auto const this_package_info = get_package_info(identifier);
  if (not this_package_info)
    throw std::runtime_error(
      std::format("package {} does not contain a manifest.json?", identifier));

  ctx->add_package(identifier, this_package_info->type);

  for (auto const& internal : this_package_info->internal_dependencies) {
    auto const internal_identifier = internal.packageIdentifier();
    add_to_dependency_tree(ctx, db, visited_packages, internal_identifier);
    ctx->add_dependency(identifier, internal, false);
  }

  for (auto const& external : this_package_info->external_dependencies) {
    auto const external_identifier = external.packageIdentifier();
    add_to_dependency_tree(ctx, db, visited_packages, external_identifier);
    ctx->add_dependency(identifier, external, true);
  }
}

Deptree
build_dependency_tree(ConfigurationFile const& config,
                      PackageCacheDB const& db,
                      TargetTriplet const& this_target)
{
  std::unique_ptr<DeptreeCtx, DeptreeDeleter> ctx(new DeptreeCtx);

  std::set<PackageIdentifier> visited_packages;
  auto const this_package_identifier =
    get_this_package_ident(config, this_target);
  visited_packages.insert(this_package_identifier);

  /*

    add packages to the tree,
    and also the dependencies as edges

  */

  ctx->add_package(config.project.org,
                   config.project.name,
                   config.project.version,
                   this_target.to_string(),
                   config.meta.type);

  for (auto const& internal : config.depends.internal) {
    auto const dependency_identifier = parse_dependency_identifier(internal);

    if (not dependency_identifier)
      throw std::runtime_error(std::format(
        "invalid dependency identifier in internal dependencies, '{}'",
        internal));

    auto const depend_package_ident =
      dependency_identifier->packageIdentifier();

    add_to_dependency_tree(
      ctx.get(), db, visited_packages, depend_package_ident);
    ctx->add_dependency(this_package_identifier, *dependency_identifier, false);
  }

  for (auto const& external : config.depends.external) {
    auto const dependency_identifier = parse_dependency_identifier(external);

    if (not dependency_identifier)
      throw std::runtime_error(std::format(
        "invalid dependency identifier in external dependencies, '{}'",
        external));

    auto const depend_package_ident =
      dependency_identifier->packageIdentifier();

    add_to_dependency_tree(
      ctx.get(), db, visited_packages, depend_package_ident);
    ctx->add_dependency(this_package_identifier, *dependency_identifier, true);
  }

  /*
    run the datalog engine!
    for large trees, this is where all of the
    perf goes
  */
  ctx->interpreter.infer();
  threadsafe_print_verbose("--DUMPING DEPEDENCY GRAPH INTERPRETER DATA--\n",
                           ctx->interpreter.dump_facts());

  /*
    checks!
  */

  using namespace datalogpp;

  {
    /*
      simple, easy to rule out stuff
    */

    if (ctx->interpreter.query(std::array{ "StaticToStaticDependencies"_p() })
          .size() != 0)
      throw std::runtime_error(
        "static to static dependencies are currently not allowed by hewg");

    // sibling dependencies that which share the same org and name,
    // perhaps differing in version are disallowed
    if (ctx->interpreter.query(std::array{ "RepeatDependencySiblings"_p() })
          .size() != 0)
      throw std::runtime_error(
        "a dependency is repeated as a sibling somewhere in the tree");

    if (ctx->interpreter.query(std::array{ "ExistsExternalDependency"_p() })
          .size() != 0)
      throw std::runtime_error(
        "external dependencies are currently not allowed by hewg");

    if (ctx->interpreter.query(std::array{ "DependencyCycle"_p() }).size() != 0)
      throw std::runtime_error("loop detected in package depedency graph");
  }

  {
    /*
      more complicated checks against the versions...
    */

    /*
      NOTE:
        currently, shared libraries are unsupported
        re-exportation is also unsupported
        therefore, there are very few checks we need to make

      TODO:
        * forbid >= versions on major version 0 semver
    */

    auto const packages = ctx->interpreter.query(std::array{
      "Package"_p("ORG"_V, "NAME"_V, "VERSION"_V, "TARGET"_V, "_"_V) });

    // auto const equal_versions = ctx->interpreter.query(std::array{
    //   "Dependency"_p(
    //     "_"_V, "_"_V, "_"_V, "_"_V, "_"_V, "_"_V, "_"_V, "_"_V, "=", "_"_V),
    // });

    // if (equal_versions.size() != 0)
    //   throw std::runtime_error(
    //     "equal dependency versions are currently not allowed by hewg");

    auto const greater_versions = ctx->interpreter.query(std::array{
      "Dependency"_p("_"_V,
                     "_"_V,
                     "_"_V,
                     "_"_V,
                     "_"_V,
                     "_"_V,
                     "DEPEND_VERSION"_V,
                     "_"_V,
                     ">=",
                     "_"_V),
    });

    for (auto const& subst : greater_versions) {
      auto const depend_version = subst.at("DEPEND_VERSION"_V);
      auto const semver = parse_semver(depend_version);

      if (not semver)
        throw std::runtime_error("malformed semver in dependency graph");

      if (semver->major() == 0)
        throw std::runtime_error("a dependent package with major version 0 may "
                                 "not be a >= dependency");
    }
  }

  /*
   * no >= versions applied on maj ver 0
   * no shared libraries allowed
   * no re-exportation allowed
   * no static->static allowed
   */

  return ctx;
}

std::set<PackageIdentifier>
collect_packages_to_include(ConfigurationFile const& config,
                            PackageCacheDB const& db,
                            TargetTriplet const& this_target,
                            Deptree const& tree)
{
  auto const this_package_identifier =
    get_this_package_ident(config, this_target);

  // auto const this_package_header_deps = ctx->interpreter.query(std::array{
  //   "Dependency"_p(this_package_identifier.org(),
  //                  this_package_identifier.name(),
  //                  std::format("{}", this_package_identifier.version()),
  //                  this_package_identifier.target().to_string(),
  //                  "CHILD_ORG"_V,
  //                  "CHILD_NAME"_V,
  //                  "CHILD_VERSION"_V,
  //                  "CHILD_TARGET"_V,
  //                  "DEPENDS"_V,
  //                  "EXT"_V),
  //   "Package"_p("CHILD_ORG"_V,
  //               "CHILD_NAME"_V,
  //               "CHILD_VERSION"_V,
  //               "CHILD_TARGET"_V,
  //               "headers") });

  auto const this_package_deps = tree->interpreter.query(std::array{
    "Dependency"_p(this_package_identifier.org(),
                   this_package_identifier.name(),
                   std::format("{}", this_package_identifier.version()),
                   this_package_identifier.target().to_string(),
                   "CHILD_ORG"_V,
                   "CHILD_NAME"_V,
                   "CHILD_VERSION"_V,
                   "CHILD_TARGET"_V,
                   "DEPENDS"_V,
                   "EXT"_V) });

  std::set<PackageIdentifier> output;

  for (auto const& subst : this_package_deps) {
    auto const org = subst.at("CHILD_ORG"_V);
    auto const name = subst.at("CHILD_NAME"_V);
    auto const version = subst.at("CHILD_VERSION"_V);
    auto const target = subst.at("CHILD_TARGET"_V);
    auto const depends = subst.at("DEPENDS"_V);

    auto const version_semver = parse_semver(version);
    auto const target_parse = TargetTriplet(target);
    auto const depends_parse = sort_from_string(depends);

    if (not version_semver)
      throw std::runtime_error("malformed semver in package graph");
    if (not depends_parse)
      throw std::runtime_error("malformed sort in package graph");

    auto const package_ident =
      PackageIdentifier(org, name, *version_semver, target_parse);

    auto const selected_version = select_package_from_dependency_identifier(
      db, DependencyIdentifier(*depends_parse, package_ident));

    if (not selected_version)
      throw std::runtime_error(std::format(
        "unable to select valid version for package {}", package_ident));

    output.insert(*selected_version);
  }

  return output;
}

std::set<PackageIdentifier>
collect_packages_to_link(ConfigurationFile const& config,
                         PackageCacheDB const& db,
                         TargetTriplet const& this_target,
                         Deptree const& tree)
{
  auto const this_package_identifier =
    get_this_package_ident(config, this_target);

  auto const this_package_static_dependencies =
    tree->interpreter.query(std::array{
      "Dependency"_p(this_package_identifier.org(),
                     this_package_identifier.name(),
                     std::format("{}", this_package_identifier.version()),
                     this_package_identifier.target().to_string(),
                     "CHILD_ORG"_V,
                     "CHILD_NAME"_V,
                     "CHILD_VERSION"_V,
                     "CHILD_TARGET"_V,
                     "DEPENDS"_V,
                     "EXT"_V),
      "Package"_p("CHILD_ORG"_V,
                  "CHILD_NAME"_V,
                  "CHILD_VERSION"_V,
                  "CHILD_TARGET"_V,
                  "library") });

  std::set<PackageIdentifier> output;

  for (auto const& subst : this_package_static_dependencies) {
    auto const org = subst.at("CHILD_ORG"_V);
    auto const name = subst.at("CHILD_NAME"_V);
    auto const version = subst.at("CHILD_VERSION"_V);
    auto const target = subst.at("CHILD_TARGET"_V);
    auto const depends = subst.at("DEPENDS"_V);

    auto const version_semver = parse_semver(version);
    auto const target_parse = TargetTriplet(target);
    auto const depends_parse = sort_from_string(depends);

    if (not version_semver)
      throw std::runtime_error("malformed semver in package graph");
    if (not depends_parse)
      throw std::runtime_error("malformed sort in package graph");

    auto const package_ident =
      PackageIdentifier(org, name, *version_semver, target_parse);

    auto const selected_version = select_package_from_dependency_identifier(
      db, DependencyIdentifier(*depends_parse, package_ident));

    if (not selected_version)
      throw std::runtime_error(std::format(
        "unable to select valid version for package {}", package_ident));

    output.insert(*selected_version);
  }

  return output;
}
