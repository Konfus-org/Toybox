#pragma once
#include "tbx/core/interfaces/file_ops.h"
#include "tbx/core/interfaces/message_dispatcher.h"
#include "tbx/core/systems/plugin_api/loaded_plugin.h"
#include "tbx/core/tbx_api.h"
#include "tbx/core/systems/time/delta_time.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    // Returns true when the path matches the platform-specific plugin manifest filename pattern.
    TBX_API bool is_plugin_manifest_path(const std::filesystem::path& path);

    // Resolves the concrete plugin library path that should be loaded for the given metadata.
    TBX_API std::filesystem::path resolve_plugin_library_path(
        const PluginMeta& meta,
        IFileOps& file_ops);

    // Scans 'directory' for plugin manifests (e.g. `*.dll.meta`, `*.so.meta`),
    // skips any `resources/` subtree, filters by requested IDs,
    // resolves load order, loads plugins, and returns owned plugin containers.
    // Ownership: The caller owns the returned LoadedPlugin objects.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API std::vector<LoadedPlugin> load_plugins(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_ids,
        IFileOps& file_ops);

    // Scans 'directory' for plugin manifests using a working directory-backed file operator.
    // Ownership: The caller owns the returned LoadedPlugin objects.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API std::vector<LoadedPlugin> load_plugins(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_ids,
        const std::filesystem::path& working_directory);

    // Loads plugins from already-parsed metadata, without any file IO.
    // Ownership: The caller owns the returned LoadedPlugin objects.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API std::vector<LoadedPlugin> load_plugins(
        const std::vector<PluginMeta>& metas,
        IFileOps& file_ops);

    // Loads plugins from already-parsed metadata using a working directory-backed file operator.
    // Ownership: The caller owns the returned LoadedPlugin objects.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API std::vector<LoadedPlugin> load_plugins(
        const std::vector<PluginMeta>& metas,
        const std::filesystem::path& working_directory);

    // Unloads plugins in a deterministic dependency-aware order.
    // Ownership: Consumes and destroys LoadedPlugin instances in the provided vector.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API void unload_plugins(
        std::vector<LoadedPlugin>& loaded_plugins,
        IMessageCoordinator* coordinator = nullptr);

    /// @brief
    /// Purpose: Updates loaded plugins in deterministic category/priority order for
    /// variable-timestep frame updates.
    /// @details
    /// Ownership: Does not take ownership of plugin instances.
    /// Thread Safety: Not thread-safe; call from the main thread.
    TBX_API void update_plugins(std::vector<LoadedPlugin>& loaded_plugins, const DeltaTime& dt);

    /// @brief
    /// Purpose: Updates loaded plugins in deterministic category/priority order for fixed-timestep
    /// simulation updates.
    /// @details
    /// Ownership: Does not take ownership of plugin instances.
    /// Thread Safety: Not thread-safe; call from the main thread.
    TBX_API void update_plugins_fixed(
        std::vector<LoadedPlugin>& loaded_plugins,
        const DeltaTime& dt);
}
