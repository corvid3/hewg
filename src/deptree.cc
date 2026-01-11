#include <algorithm>
#include <crow.datalogpp/datalogpp.hh>
#include <print>
#include <set>
#include <stdexcept>

#include "analysis.hh"
#include "deptree.hh"
#include "packages.hh"

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
    &dependency_cycle, &extern_chain, &static_domain, &dynamic_dependency,
    &include_headers;

  auto collect_static_domain(PackageIdentifier const& ident)
  {
    return interpreter.query(
      std::array{ "StaticDomain"_p(ident.org(),
                                   ident.name(),
                                   std::format("{}", ident.version()),
                                   ident.target().to_string(),
                                   "ORG"_V,
                                   "NAME"_V,
                                   "VERSION"_V,
                                   "TARGET"_V) });
  }

  auto collect_packages_by_name_in_static_domain(
    PackageIdentifier const& owner_ident,
    std::string org,
    std::string name,
    std::string target)
  {
    return interpreter.query(
      std::array{ "StaticDomain"_p(owner_ident.org(),
                                   owner_ident.name(),
                                   std::format("{}", owner_ident.version()),
                                   owner_ident.target().to_string(),
                                   org,
                                   name,
                                   "VERSION"_V,
                                   target) });
  }

  auto collect_packages_to_include(PackageIdentifier const& owner_ident)
  {
    return interpreter.query(
      std::array{ "IncludeHeaders"_p(owner_ident.org(),
                                     owner_ident.name(),
                                     std::format("{}", owner_ident.version()),
                                     owner_ident.target().to_string(),
                                     "ORG"_V,
                                     "NAME"_V,
                                     "VERSION"_V,
                                     "TARGET"_V) });
  }

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
    , static_domain(interpreter.predicate("StaticDomain", 8))
    , dynamic_dependency(interpreter.predicate("DynamicDependency", 8))
    , include_headers(interpreter.predicate("IncludeHeaders", 8))
  {
    using namespace datalogpp;

    static_domain("HEAD_ORG"_V,
                  "HEAD_NAME"_V,
                  "HEAD_VERSION"_V,
                  "HEAD_TARGET"_V,
                  "CHILD_ORG"_V,
                  "CHILD_NAME"_V,
                  "CHILD_VERSION"_V,
                  "CHILD_TARGET"_V) =
      "Dependency"_p("HEAD_ORG"_V,
                     "HEAD_NAME"_V,
                     "HEAD_VERSION"_V,
                     "HEAD_TARGET"_V,
                     "CHILD_ORG"_V,
                     "CHILD_NAME"_V,
                     "CHILD_VERSION"_V,
                     "CHILD_TARGET"_V,
                     "_"_V,
                     "_"_V) +
      "Package"_p("CHILD_ORG"_V,
                  "CHILD_NAME"_V,
                  "CHILD_VERSION"_V,
                  "CHILD_TARGET"_V,
                  "CHILD_TYPE"_V) +
      /* static domains end at dynlibs and executables */
      Inequality("CHILD_TYPE"_V, "dynlib") +
      Inequality("CHILD_TYPE"_V, "executable");

    static_domain("HEAD_ORG"_V,
                  "HEAD_NAME"_V,
                  "HEAD_VERSION"_V,
                  "HEAD_TARGET"_V,
                  "CHILD_ORG"_V,
                  "CHILD_NAME"_V,
                  "CHILD_VERSION"_V,
                  "CHILD_TARGET"_V) =
      "StaticDomain"_p("HEAD_ORG"_V,
                       "HEAD_NAME"_V,
                       "HEAD_VERSION"_V,
                       "HEAD_TARGET"_V,
                       "INTER_ORG"_V,
                       "INTER_NAME"_V,
                       "INTER_VERISON"_V,
                       "INTER_TARGET"_V) +
      "Dependency"_p("INTER_ORG"_V,
                     "INTER_NAME"_V,
                     "INTER_VERSION"_V,
                     "INTER_TARGET"_V,
                     "CHILD_ORG"_V,
                     "CHILD_NAME"_V,
                     "CHILD_VERSION"_V,
                     "CHILD_TARGET"_V,
                     "_"_V,
                     "_"_V) +
      "Package"_p("CHILD_ORG"_V,
                  "CHILD_NAME"_V,
                  "CHILD_VERSION"_V,
                  "CHILD_TARGET"_V,
                  "CHILD_TYPE"_V) +
      /* static domains end at dynlibs and executables */
      Inequality("CHILD_TYPE"_V, "dynlib") +
      Inequality("CHILD_TYPE"_V, "executable");

    /* a dynamic dependency is a link between static domains */
    dynamic_dependency("HEAD_ORG"_V,
                       "HEAD_NAME"_V,
                       "HEAD_VERSION"_V,
                       "HEAD_TARGET"_V,
                       "CHILD_ORG"_V,
                       "CHILD_NAME"_V,
                       "CHILD_VERSION"_V,
                       "CHILD_TARGET"_V) = "StaticDomain"_p("HEAD_ORG"_V,
                                                            "HEAD_NAME"_V,
                                                            "HEAD_VERSION"_V,
                                                            "HEAD_TARGET"_V,
                                                            "INTER_ORG"_V,
                                                            "INTER_NAME"_V,
                                                            "INTER_VERSION"_V,
                                                            "INTER_TARGET"_V) +
                                           "Dependency"_p("INTER_ORG"_V,
                                                          "INTER_NAME"_V,
                                                          "INTER_VERSION"_V,
                                                          "INTER_TARGET"_V,
                                                          "CHILD_ORG"_V,
                                                          "CHILD_NAME"_V,
                                                          "CHILD_VERSION"_V,
                                                          "CHILD_TARGET"_V,
                                                          "_"_V,
                                                          "_"_V) +
                                           "Package"_p("CHILD_ORG"_V,
                                                       "CHILD_NAME"_V,
                                                       "CHILD_VERSION"_V,
                                                       "CHILD_TARGET"_V,
                                                       "dynlib"_V);
    dynamic_dependency("HEAD_ORG"_V,
                       "HEAD_NAME"_V,
                       "HEAD_VERSION"_V,
                       "HEAD_TARGET"_V,
                       "CHILD_ORG"_V,
                       "CHILD_NAME"_V,
                       "CHILD_VERSION"_V,
                       "CHILD_TARGET"_V) = "Dependency"_p("HEAD_ORG"_V,
                                                          "HEAD_NAME"_V,
                                                          "HEAD_VERSION"_V,
                                                          "HEAD_TARGET"_V,
                                                          "CHILD_ORG"_V,
                                                          "CHILD_NAME"_V,
                                                          "CHILD_VERSION"_V,
                                                          "CHILD_TARGET"_V,
                                                          "_"_V,
                                                          "_"_V) +
                                           "Package"_p("CHILD_ORG"_V,
                                                       "CHILD_NAME"_V,
                                                       "CHILD_VERSION"_V,
                                                       "CHILD_TARGET"_V,
                                                       "dynlib"_V);

    include_headers("HEAD_ORG"_V,
                    "HEAD_NAME"_V,
                    "HEAD_VERSION"_V,
                    "HEAD_TARGET"_V,
                    "CHILD_ORG"_V,
                    "CHILD_NAME"_V,
                    "CHILD_VERSION"_V,
                    "CHILD_TARGET"_V) = "Dependency"_p("HEAD_ORG"_V,
                                                       "HEAD_NAME"_V,
                                                       "HEAD_VERSION"_V,
                                                       "HEAD_TARGET"_V,
                                                       "CHILD_ORG"_V,
                                                       "CHILD_NAME"_V,
                                                       "CHILD_VERSION"_V,
                                                       "CHILD_TARGET"_V,
                                                       "_"_V,
                                                       "_"_V);

    include_headers("HEAD_ORG"_V,
                    "HEAD_NAME"_V,
                    "HEAD_VERSION"_V,
                    "HEAD_TARGET"_V,
                    "CHILD_ORG"_V,
                    "CHILD_NAME"_V,
                    "CHILD_VERSION"_V,
                    "CHILD_TARGET"_V) = "IncludeHeaders"_p("HEAD_ORG"_V,
                                                           "HEAD_NAME"_V,
                                                           "HEAD_VERSION"_V,
                                                           "HEAD_TARGET"_V,
                                                           "INTER_ORG"_V,
                                                           "INTER_NAME"_V,
                                                           "INTER_VERSION"_V,
                                                           "INTER_TARGET"_V) +
                                        "Dependency"_p("INTER_ORG"_V,
                                                       "INTER_NAME"_V,
                                                       "INTER_VERSION"_V,
                                                       "INTER_TARGET"_V,
                                                       "CHILD_ORG"_V,
                                                       "CHILD_NAME"_V,
                                                       "CHILD_VERSION"_V,
                                                       "CHILD_TARGET"_V,
                                                       "_"_V,
                                                       "external");

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
      "Package"_p("ORG"_V, "NAME"_V, "VERSION"_V, "TARGET"_V, "library") +
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
      "Package"_p(
        "C_ORG"_V, "C_NAME"_V, "C_VERSION"_V, "C_TARGET"_V, "library");

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
    if (ctx->interpreter.query(std::array{ "DependencyCycle"_p() }).size() != 0)
      throw std::runtime_error("loop detected in package depedency graph");
  }

  {
    for (auto const& subst :
         ctx->collect_static_domain(this_package_identifier)) {
      auto const versions =
        ctx->collect_packages_by_name_in_static_domain(this_package_identifier,
                                                       subst.at("ORG"_V),
                                                       subst.at("NAME"_V),
                                                       subst.at("TARGET"_V));

      if (versions.size() > 1)
        throw std::runtime_error(
          std::format("multiple versions specified for a dependency {}.{}",
                      subst.at("ORG"_V),
                      subst.at("NAME"_V)));
    }
  }

  {
    /*
      more complicated checks against the versions...
    */

    /*
     * check that for each and every package, there is either
     * only compatable = checks, all >=, or >= with compatable =
     */

    using DepgraphPackage =
      std::tuple<std::string, std::string, std::string, std::string>;

    std::set<DepgraphPackage> static_domain_heads;

    auto const packages = ctx->interpreter.query(std::array{
      "Package"_p("ORG"_V, "NAME"_V, "VERSION"_V, "TARGET"_V, "_"_V) });

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
   */

  return ctx;
}

std::set<PackageIdentifier>
collect_packages_to_include(ConfigurationFile const& config,
                            PackageCacheDB const&,
                            TargetTriplet const& this_target,
                            Deptree const& tree)
{
  auto const this_package_identifier =
    get_this_package_ident(config, this_target);

  std::set<PackageIdentifier> output;

  for (auto const& subst :
       tree->collect_packages_to_include(this_package_identifier)) {
    auto const org = subst.at("ORG"_V);
    auto const name = subst.at("NAME"_V);
    auto const version = subst.at("VERSION"_V);
    auto const target = subst.at("TARGET"_V);

    auto const version_semver = parse_semver(version);
    auto const target_parse = TargetTriplet(target);

    if (not version_semver)
      throw std::runtime_error("malformed semver in package graph");

    auto const package_ident =
      PackageIdentifier(org, name, *version_semver, target_parse);

    output.insert(package_ident);
  }

  return output;
}

std::set<PackageIdentifier>
collect_packages_to_link(ConfigurationFile const& config,
                         PackageCacheDB const&,
                         TargetTriplet const& this_target,
                         Deptree const& tree)
{
  auto const this_package_identifier =
    get_this_package_ident(config, this_target);

  std::set<PackageIdentifier> output;

  for (auto const& subst :
       tree->collect_static_domain(this_package_identifier)) {
    auto const org = subst.at("ORG"_V);
    auto const name = subst.at("NAME"_V);
    auto const version = subst.at("VERSION"_V);
    auto const target = subst.at("TARGET"_V);

    auto const package_query = tree->interpreter.query(
      std::array{ "Package"_p(org, name, version, target, "TYPE"_V) });

    /* dont want to link headers */
    if (package_query.front().at("TYPE"_V) == "headers")
      continue;

    auto const version_semver = parse_semver(version);
    auto const target_parse = TargetTriplet(target);

    if (not version_semver)
      throw std::runtime_error("malformed semver in package graph");

    auto const package_ident =
      PackageIdentifier(org, name, *version_semver, target_parse);

    output.insert(package_ident);
  }

  return output;
}
