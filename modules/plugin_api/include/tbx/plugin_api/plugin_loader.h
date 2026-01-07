#pragma once
#include "tbx/files/filesystem.h"
#include "tbx/plugin_api/loaded_plugin.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    class TBX_API PluginLoader
    {
      public:
        // Scans 'directory' for manifests (*.meta or plugin.meta), filters by requested IDs,
        // resolves load order, loads plugins, and returns pointers to loaded plugins.
        // Ownership: The caller owns the returned LoadedPlugin objects.
        // Thread-safety: Not thread-safe; call from the main thread.
        std::vector<LoadedPlugin> load_plugins(
            const std::filesystem::path& directory,
            const std::vector<std::string>& requested_ids,
            IFileSystem& file_ops);

        // Loads plugins from already-parsed metadata, without any filesystem IO.
        // Ownership: The caller owns the returned LoadedPlugin objects.
        // Thread-safety: Not thread-safe; call from the main thread.
        std::vector<LoadedPlugin> load_plugins(
            const std::vector<PluginMeta>& metas,
            IFileSystem& file_ops);
    };
}
