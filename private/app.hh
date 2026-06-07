#pragma once

#include "cmdline.hh"
#include "common.hh"
#include "confs.hh"
#include "target.hh"
#include "thread_pool.hh"

/* contains global information about the project invoked,
 * as well as threading and other OS specific subsystems */
struct AppContext {
public:
  /* takes in command line arguments */
  AppContext(int argc, char** argv);

  auto
  options() const -> ToplevelOptions const&
  {
    return m_options;
  }

  auto
  bares() const -> auto&
  {
    return options().terse_bares;
  }

  auto
  build_options() const -> BuildOptions const&
  {
    return std::get<BuildOptions>(m_options.terse_subcmds);
  }

  auto
  init_options() const -> InitOptions const&
  {
    return std::get<InitOptions>(m_options.terse_subcmds);
  }

  auto
  clean_options() const -> CleanOptions const&
  {
    return std::get<CleanOptions>(m_options.terse_subcmds);
  }

  auto
  threads() const -> ThreadPool&
  {
    return m_threads;
  }

  auto
  config() const -> ConfigurationFile const&
  {
    return m_config;
  }

  auto
  triplet() const -> TargetTriplet const&
  {
    return m_triplet;
  }

private:
  ToplevelOptions    m_options;
  mutable ThreadPool m_threads;
  TargetTriplet      m_triplet;
  ConfigurationFile  m_config;
};

inline AppContext::AppContext(int argc, char** argv)
  : m_options(parse_cmdline(argc, argv))
  , m_threads(m_options.num_tasks)
  , m_triplet(TargetTriplet(m_options.target.value_or(THIS_TARGET)))
  , m_config(get_config_file(m_options,
                             m_triplet,
                             m_options.config_file_path.value_or("./hewg.scl")))
{
  verbose_output = options().verbose_print;
  skip_countdown = options().skip_pause;
  num_tasks      = m_options.num_tasks;
}
