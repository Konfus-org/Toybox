#pragma once
#include "tbx/plugin_api/loaded_plugin.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    // Scans 'directory' for manifests (*.meta or plugin.meta), filters by requested IDs,
    // resolves load order, loads plugins, and returns pointers to loaded plugins.
    // Ownership: The caller owns the returned LoadedPlugin* objects and is
    // responsible for deleting them when done.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API std::vector<LoadedPlugin*> load_plugins(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_ids);

    // Loads plugins from already-parsed metadata, without any filesystem IO.
    // Ownership: The caller owns the returned LoadedPlugin* objects and is
    // responsible for deleting them when done.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API std::vector<LoadedPlugin*> load_plugins(const std::vector<PluginMeta>& metas);
}
