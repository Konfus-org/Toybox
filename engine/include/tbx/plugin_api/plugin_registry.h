#pragma once
#include "tbx/tbx_api.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx
{
    struct Plugin;

    // Global registry tracking plugin entry points and live instances.
    // Ownership: Does not own LoadedPlugin instances; callers manage lifetimes.
    // Thread-safety: Not thread-safe; access must be serialized on the main thread.
    class TBX_API PluginRegistry
    {
       public:
        static PluginRegistry& get_instance();

        // Registers a plugin instance. Must be called from the main thread. The
        // registry does not take ownership of the pointer and expects the
        // caller to manage its lifetime.
        void register_plugin(Plugin* plugin);

        // Unregisters a plugin instance. Must be called from the main thread.
        // The registry does not delete the pointer.
        void unregister_plugin(Plugin* plugin);

        // Returns a list of all currently registered plugins.
        std::vector<Plugin*> get_registered_plugins() const;

      private:
        std::vector<Plugin*> _plugins;
    };
}
