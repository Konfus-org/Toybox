#pragma once
#include "tbx/tbx_api.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx
{
    class Plugin;

    // The registry observes plugin instances but never owns them — they live in unique_ptrs in the
    // plugin loader (see plugin_loader.cpp / plugin_ownership.h). This alias makes that explicit at
    // every storage site; the loader is responsible for unregistering before it destroys a plugin.
    using NonOwningPlugin = Plugin*;

    // Global registry tracking plugin names and live instances.
    // Ownership: Does not own LoadedPlugin instances; callers manage lifetimes.
    // Thread-safety: Not thread-safe; access must be serialized on the main thread.
    class TBX_API PluginRegistry
    {
      public:
        static PluginRegistry& get_instance();

      public:
        PluginRegistry(const PluginRegistry&) = delete;
        PluginRegistry& operator=(const PluginRegistry&) = delete;
        PluginRegistry(PluginRegistry&&) = delete;
        PluginRegistry& operator=(PluginRegistry&&) = delete;

      public:
        // Registers a plugin instance. Must be called from the main thread. The
        // registry does not take ownership of the pointer and expects the
        // caller to manage its lifetime.
        void register_plugin(const std::string& name, Plugin* plugin);

        // Unregisters a plugin instance. Must be called from the main thread.
        // The registry does not delete the pointer.
        void unregister_plugin(const std::string& name);

        // Removes a plugin instance by pointer when the name is unknown.
        void unregister_plugin(Plugin* plugin);

        // Returns a list of all currently registered plugins.
        std::vector<Plugin*> get_registered_plugins() const;

        // Locates a plugin instance by name.
        Plugin* find_plugin(const std::string& name) const;

        /// @brief Purpose: Returns the registered name for a plugin instance.
        /// @details Ownership: Returns an owned string that may be empty if not registered.
        /// Thread Safety: Not thread-safe; call from the main thread.
        std::string get_registered_name(const Plugin* plugin) const;

      private:
        PluginRegistry() = default;
        ~PluginRegistry() noexcept = default;

      private:
        std::vector<NonOwningPlugin> _plugins;
        std::unordered_map<std::string, NonOwningPlugin> _plugins_by_name;
    };
}
