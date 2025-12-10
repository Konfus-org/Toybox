#pragma once
#include "tbx/common/collections.h"
#include "tbx/common/string.h"
#include "tbx/tbx_api.h"

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
        void register_plugin(const String& name, Plugin* plugin);

        // Unregisters a plugin instance. Must be called from the main thread.
        // The registry does not delete the pointer.
        void unregister_plugin(const String& name);

        // Removes a plugin instance by pointer when the name is unknown.
        void unregister_plugin(Plugin* plugin);

        // Returns a list of all currently registered plugins.
        List<Plugin*> get_registered_plugins() const;

        // Locates a plugin instance by name.
        Plugin* find_plugin(const String& name) const;

      private:
        List<Plugin*> _plugins;
        HashMap<String, Plugin*> _plugins_by_name;
    };
}
