#pragma once
#include "tbx/plugin_api/loaded_plugin.h"
#include "tbx/plugin_api/plugin_host.h"
#include "tbx/tbx_api.h"
#include "tbx/time/delta_time.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    // Scans 'directory' for plugin manifests (e.g. `*.dll.meta`, `*.so.meta`),
    // skips any `resources/` subtree, filters by requested IDs,
    // resolves load order, loads plugins, and returns pointers to loaded plugins.
    // Ownership: The caller owns the returned LoadedPlugin objects. The host must outlive
    // the returned plugins.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API std::vector<LoadedPlugin> load_plugins(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_ids,
        const std::filesystem::path& working_directory,
        IPluginHost& host);

    // Loads plugins from already-parsed metadata, without any file IO.
    // Ownership: The caller owns the returned LoadedPlugin objects. The host must outlive
    // the returned plugins.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API std::vector<LoadedPlugin> load_plugins(
        const std::vector<PluginMeta>& metas,
        const std::filesystem::path& working_directory,
        IPluginHost& host);

    // Unloads plugins in a deterministic dependency-aware order.
    // Ownership: Consumes and destroys LoadedPlugin instances in the provided vector.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API void unload_plugins(std::vector<LoadedPlugin>& loaded_plugins);

    /// <summary>
    /// Purpose: Updates loaded plugins in deterministic category/priority order for
    /// variable-timestep frame updates.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not take ownership of plugin instances.
    /// Thread Safety: Not thread-safe; call from the main thread.
    /// </remarks>
    TBX_API void update_plugins(std::vector<LoadedPlugin>& loaded_plugins, const DeltaTime& dt);

    /// <summary>
    /// Purpose: Updates loaded plugins in deterministic category/priority order for fixed-timestep
    /// simulation updates.
    /// </summary>
    /// <remarks>
    /// Ownership: Does not take ownership of plugin instances.
    /// Thread Safety: Not thread-safe; call from the main thread.
    /// </remarks>
    TBX_API void update_plugins_fixed(
        std::vector<LoadedPlugin>& loaded_plugins,
        const DeltaTime& dt);
}
