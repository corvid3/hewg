#include "common.hh"
#include "confs.hh"
#include "hooks.hh"
#include "paths.hh"
#include "thread_pool.hh"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <jayson.hh>

struct HookCache
{
  std::vector<std::string> once_hooks_ran;

  using jayson_fields =
    std::tuple<jayson::obj_field<"once_hooks", &HookCache::once_hooks_ran>>;
};

class HookCacheAccess
{
public:
  static HookCache& get_cache()
  {
    if (not m_cache) {
      m_cache = open_hook_cache();
      std::atexit([]() { write_hook_cache(*m_cache); });
    }

    return *m_cache;
  }

private:
  static HookCache open_hook_cache()
  {
    if (not std::filesystem::exists(hewg_hook_cache_path))
      return {};

    std::stringstream ss;
    ss << std::ifstream(hewg_hook_cache_path).rdbuf();

    jayson::val v = jayson::val::parse(std::move(ss).str());
    HookCache out;

    jayson::deserialize(v, out);

    return out;
  }

  static void write_hook_cache(HookCache cache)
  {
    std::ofstream(hewg_hook_cache_path)
      << (jayson::serialize(cache).serialize());
  }

  static inline std::optional<HookCache> m_cache;
};

static void
trigger_once(HookCache& cache, HooksConf const& hooks)
{
  for (auto const& hook : hooks.once) {
    if (not std::ranges::contains(cache.once_hooks_ran, hook)) {
      run_command("sh", (hewg_hook_path / hook).string());
      cache.once_hooks_ran.push_back(hook);
    }
  }
}

static void
trigger_all(HooksConf const& hooks)
{
  for (auto const& hook : hooks.always)
    run_command("sh", (hewg_hook_path / hook).string());
}

void
trigger_prebuild_hooks(ConfigurationFile const& config)
{
  auto& cache = HookCacheAccess::get_cache();
  trigger_once(cache, config.prebuild_hooks);
  trigger_all(config.prebuild_hooks);
}

void
triggers_postbuild_hooks(ConfigurationFile const& config)
{
  auto& cache = HookCacheAccess::get_cache();
  trigger_once(cache, config.postbuild_hooks);
  trigger_all(config.postbuild_hooks);
}
