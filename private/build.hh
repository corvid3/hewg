#pragma once

#include "analysis.hh"
#include "app.hh"
#include "cmdline.hh"
#include "compile.hh"
#include "confs.hh"
#include "deptree.hh"
#include "packages.hh"
#include "target.hh"
#include "thread_pool.hh"

#include <string>

class PackageContext
{
public:
  explicit PackageContext(AppContext const& ctx);

  [[nodiscard]]
  auto
  db() const -> auto&
  {
    return m_db;
  }
  [[nodiscard]]
  auto
  tree() const -> auto const&
  {
    return m_tree;
  }
  [[nodiscard]]
  auto
  includes() const -> auto const&
  {
    return m_includedPackages;
  }

private:
  mutable PackageCacheDB      m_db;
  Deptree                     m_tree;
  std::set<PackageIdentifier> m_includedPackages;
};

class BuildContext
{
public:
  explicit BuildContext(AppContext const&     ctx,
                        PackageContext const& pkg,
                        bool                  PIC);

  [[nodiscard]]
  auto
  target() const -> auto const&
  {
    return m_target;
  }

  [[nodiscard]]
  auto
  ident() const -> auto const&
  {
    return m_thisIdent;
  }

  [[nodiscard]]
  auto
  cflags() const -> auto const&
  {
    return m_cFlags;
  }

  [[nodiscard]]
  auto
  cxxflags() const -> auto const&
  {
    return m_cxxFlags;
  }

  [[nodiscard]]
  auto
  cache() const -> auto&
  {
    return m_cacheFolder;
  }

  [[nodiscard]]
  auto
  emitdir() const -> auto&
  {
    return m_outputDir;
  }

  [[nodiscard]]
  auto
  pic() const
  {
    return m_pic;
  }

private:
  TargetFile            m_target;
  PackageIdentifier     m_thisIdent;
  std::filesystem::path m_cacheFolder;
  std::filesystem::path m_outputDir;
  bool                  m_pic;

  /* common CXX flags generated at runtime */
  std::vector<std::string> m_cxxFlags;

  /* common C flags generated at runtime */
  std::vector<std::string> m_cFlags;
};

void
build(AppContext const&);
