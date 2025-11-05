#pragma once
#include "tbx/tbx_api.h"
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx
{
    class Plugin;

    using CreatePluginFn = Plugin*(*)();
    using DestroyPluginFn = void (*)(Plugin*);

    struct StaticPluginEntry
    {
        CreatePluginFn create = nullptr;
        DestroyPluginFn destroy = nullptr;
    };

    class TBX_API PluginRegistry
    {
       public:
        static PluginRegistry& instance();

        // Registers a plugin instance. Must be called from the main thread. The
        // registry does not take ownership of the plugin pointer and expects the
        // caller to manage its lifetime.
        void register_plugin(const std::string& entry_point, Plugin* plugin);

        // Unregisters a plugin instance. Must be called from the main thread.
        // The registry does not delete the plugin pointer.
        void unregister_plugin(Plugin* plugin);

        std::vector<Plugin*> get_active_plugins() const;

        void register_static_plugin_entry(
            std::string entry_point,
            CreatePluginFn create,
            DestroyPluginFn destroy);

        void unregister_static_plugin_entry(const std::string& entry_point);

        std::optional<StaticPluginEntry> find_static_plugin_entry(const std::string& entry_point) const;

       private:
        PluginRegistry() = default;
        PluginRegistry(const PluginRegistry&) = delete;
        PluginRegistry& operator=(const PluginRegistry&) = delete;

        mutable std::mutex _mutex;
        std::unordered_map<Plugin*, std::string> _plugins;
        std::unordered_map<std::string, StaticPluginEntry> _static_entries;
    };
}
