#pragma once
#include "tbx/tbx_api.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx
{
    class Plugin;

    // Global registry tracking plugin names and live instances.
    // Ownership: Does not own LoadedPlugin instances; callers manage lifetimes.
    // Thread-safety: Not thread-safe; access must be serialized on the main thread.
    class TBX_API PluginRegistry
    {
      public:
        static PluginRegistry& get_instance();

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

        /// <summary>Purpose: Returns the registered name for a plugin instance.</summary>
        /// <remarks>Ownership: Returns an owned string that may be empty if not registered.
        /// Thread Safety: Not thread-safe; call from the main thread.</remarks>
        std::string get_registered_name(const Plugin* plugin) const;

      private:
        std::vector<Plugin*> _plugins;
        std::unordered_map<std::string, Plugin*> _plugins_by_name;
    };
}
