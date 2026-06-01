#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/plugin_api/loaded_plugin.h"
#include "tbx/systems/time/delta_time.h"

namespace tbx
{
    // Returns true while the loader is probing plugin metadata by temporarily loading modules.
    // Callers can use this to ignore global registration side effects from discovery loads.
    TBX_API bool is_plugin_meta_query_active();

    // Returns true when the path matches the platform-specific plugin library filename pattern.
    TBX_API bool is_plugin_library_path(const std::filesystem::path& path);

    // Resolves the concrete plugin library path that should be loaded for the given metadata.
    TBX_API std::filesystem::path resolve_plugin_library_path(
        const PluginMeta& meta,
        IFileOps& file_ops);

    // Queries metadata from a plugin library without creating the plugin instance.
    TBX_API bool try_query_plugin_meta_from_library(
        const std::filesystem::path& library_path,
        IFileOps& file_ops,
        PluginMeta& out_meta);

    // Scans 'directory' for plugin libraries (e.g. `*.dll`, `*.so`),
    // skips any `resources/` subtree, filters by requested IDs,
    // resolves load order, loads plugins, and returns owned plugin containers.
    // Ownership: The caller owns the returned LoadedPlugin nodes.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API LoadedPlugins load_plugins(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_ids,
        IFileOps& file_ops);

    // Scans 'directory' for plugin manifests using a working directory-backed file operator.
    // Ownership: The caller owns the returned LoadedPlugin nodes.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API LoadedPlugins load_plugins(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_ids,
        const std::filesystem::path& working_directory);

    // Loads plugins from already-parsed metadata, without any file IO.
    // Ownership: The caller owns the returned LoadedPlugin nodes.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API LoadedPlugins load_plugins(
        const std::vector<PluginMeta>& metas,
        IFileOps& file_ops);

    // Loads plugins from already-parsed metadata using a working directory-backed file operator.
    // Ownership: The caller owns the returned LoadedPlugin nodes.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API LoadedPlugins load_plugins(
        const std::vector<PluginMeta>& metas,
        const std::filesystem::path& working_directory);

    // Detaches plugins in a deterministic dependency-aware order without unloading libraries.
    // Ownership: Retains LoadedPlugin instances in the provided list.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API void detach_plugins(
        LoadedPlugins& loaded_plugins,
        ServiceProvider& service_provider,
        IMessageCoordinator* coordinator = nullptr);

    // Unloads plugins in a deterministic dependency-aware order.
    // Ownership: Consumes and destroys LoadedPlugin instances in the provided list.
    // Thread-safety: Not thread-safe; call from the main thread.
    TBX_API void unload_plugins(
        LoadedPlugins& loaded_plugins,
        ServiceProvider& service_provider,
        IMessageCoordinator* coordinator = nullptr);

    /// @brief
    /// Purpose: Updates loaded plugins in deterministic category/priority order for
    /// variable-timestep frame updates.
    /// @details
    /// Ownership: Does not take ownership of plugin instances.
    /// Thread Safety: Not thread-safe; call from the main thread.
    TBX_API void update_plugins(LoadedPlugins& loaded_plugins, const DeltaTime& dt);

    /// @brief
    /// Purpose: Updates loaded plugins in deterministic category/priority order for fixed-timestep
    /// simulation updates.
    /// @details
    /// Ownership: Does not take ownership of plugin instances.
    /// Thread Safety: Not thread-safe; call from the main thread.
    TBX_API void update_plugins_fixed(
        LoadedPlugins& loaded_plugins,
        const DeltaTime& dt);
}
