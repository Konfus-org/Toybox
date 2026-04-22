#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/files/messages.h"
#include "tbx/systems/files/watcher.h"
#include "tbx/systems/plugin_api/loaded_plugin.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>


namespace tbx
{
    /// @brief
    /// Purpose: Owns loaded plugins for an application and manages their runtime lifecycle.
    /// @details
    /// Ownership: Owns all loaded plugin containers while using the provider for attachment and
    /// resource registration.
    /// Thread Safety: Not thread-safe; call from the main thread.
    class TBX_API PluginManager
    {
      public:
        PluginManager(ServiceProvider& service_provider, std::shared_ptr<IFileOps> file_ops = {});
        ~PluginManager() noexcept;

      public:
        PluginManager(const PluginManager&) = delete;
        PluginManager& operator=(const PluginManager&) = delete;
        PluginManager(PluginManager&&) = delete;
        PluginManager& operator=(PluginManager&&) = delete;

      public:
        /// @brief
        /// Purpose: Loads plugins from disk, attaches them to the service provider, and begins
        /// routing
        /// messages to them.
        /// @details
        /// Ownership: Uses the bound service-provider and file-ops instances.
        /// Thread Safety: Not thread-safe; call from the main thread.
        void load(
            const std::filesystem::path& directory,
            const std::vector<std::string>& requested_plugins,
            const std::filesystem::path& working_directory);

        /// @brief
        /// Purpose: Loads and adds a specific plugin from parsed metadata.
        /// @details
        /// Ownership: Uses the bound file-ops instance to load plugin artifacts.
        /// Thread Safety: Not thread-safe; call from the main thread.
        bool load(const PluginMeta& meta);

        /// @brief
        /// Purpose: Adds, attaches, and begins routing messages to a specific loaded plugin.
        /// @details
        /// Ownership: Takes ownership of the provided loaded plugin container.
        /// Thread Safety: Not thread-safe; call from the main thread.
        void add(LoadedPlugin loaded_plugin);

        /// @brief
        /// Purpose: Updates all managed plugins using variable-timestep ordering.
        /// @details
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Not thread-safe; call from the main thread.
        void update(const DeltaTime& dt);

        /// @brief
        /// Purpose: Updates all managed plugins using fixed-timestep ordering.
        /// @details
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Not thread-safe; call from the main thread.
        void fixed_update(const DeltaTime& dt);

        /// @brief
        /// Purpose: Detaches and unloads a specific plugin and any loaded dependents.
        /// @details
        /// Ownership: Releases ownership for unloaded plugin containers.
        /// Thread Safety: Not thread-safe; call from the main thread.
        bool unload(const std::string& plugin_name);

        /// @brief
        /// Purpose: Detaches, unloads, and stops routing messages to all managed plugins.
        /// @details
        /// Ownership: Releases owned plugin containers.
        /// Thread Safety: Not thread-safe; call from the main thread.
        void unload_all();

        /// @brief
        /// Purpose: Routes a dispatched message to all loaded plugins.
        /// @details
        /// Ownership: Does not take ownership of the message.
        /// Thread Safety: Not thread-safe; call from the main thread.
        void receive_message(Message& msg);

      private:
        bool should_load_plugin(const std::string& plugin_name) const;
        void process_pending_file_changes();
        bool try_parse_plugin_meta(const std::filesystem::path& manifest_path, PluginMeta& out_meta)
            const;
        void process_file_change(
            const FileWatchChange& change,
            std::unordered_set<std::string>& processed_plugin_names);

      private:
        std::mutex _pending_file_changes_mutex = {};
        std::vector<FileWatchChange> _pending_file_changes = {};
        std::vector<LoadedPlugin> _loaded = {};
        std::vector<std::string> _requested_plugins = {};

        std::filesystem::path _directory = {};
        std::filesystem::path _working_directory = {};

        std::shared_ptr<IFileOps> _provided_file_ops = nullptr;
        std::shared_ptr<IFileOps> _file_ops = nullptr;
        std::unique_ptr<FileWatcher> _watcher = {};

        ServiceProvider& _service_provider;
    };
}
