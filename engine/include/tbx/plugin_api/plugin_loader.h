#pragma once
#include "tbx/plugin_api/loaded_plugin.h"
#include "tbx/plugin_api/plugin.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    // Scans 'directory' for manifests (*.meta or plugin.meta), filters by requested IDs,
    // resolves load order, loads dynamic plugins, and returns ownership to the caller.
    std::vector<LoadedPlugin> load_plugins(const std::filesystem::path& directory,
                                           const std::vector<std::string>& requested_ids);

    // Loads plugins from already-parsed metadata, without any filesystem IO.
    std::vector<LoadedPlugin> load_plugins(const std::vector<PluginMeta>& metas);
}
