#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginManager.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/EventCarrier.h"
#include "Tbx/Events/PluginEvents.h"
#include <mutex>
#include <unordered_map>

namespace Tbx
{
    struct LoadedPlugin
    {
        WeakRef<Plugin> Instance = {};
        ExclusiveRef<SharedLibrary> Library = nullptr;
        PluginMeta Meta = {};
    };

    struct PluginLibraryState
    {
        std::unordered_map<const Plugin*, LoadedPlugin> Plugins = {};
        std::mutex Mutex = {};
        EventListener Listener = {};
        bool Initialized = false;
    };

    static PluginLibraryState State = {};

    static bool RemovePlugin(const Plugin* plugin, LoadedPlugin& unloaded)
    {
        if (!plugin)
        {
            return false;
        }

        std::scoped_lock lock(State.Mutex);
        auto it = State.Plugins.find(plugin);
        if (it == State.Plugins.end())
        {
            return false;
        }

        unloaded = std::move(it->second);
        State.Plugins.erase(it);
        return true;
    }

    void PluginManager::Register(const Ref<Plugin>& plugin, const PluginMeta& meta, ExclusiveRef<SharedLibrary>& library)
    {
        EnsureInitialized();

        TBX_ASSERT(plugin, "PluginLibraryManager: Cannot register a null plugin instance.");
        if (!plugin) return;

        TBX_ASSERT(library != nullptr, "PluginLibraryManager: Cannot register a plugin without its library.");
        if (!library) return;

        const auto pluginPtr = plugin.get();
        auto weakPlugin = WeakRef<Plugin>(plugin);
        auto shouldDispatchLoaded = false;

        {
            std::scoped_lock lock(State.Mutex);
            auto [it, inserted] = State.Plugins.try_emplace(pluginPtr);
            if (!inserted)
            {
                TBX_TRACE_WARNING("PluginLibraryManager: Plugin '{}' registered multiple times; refreshing metadata.", meta.Name);
            }

            it->second.Instance = weakPlugin;
            it->second.Meta = meta;
            it->second.Library = std::move(library);
            shouldDispatchLoaded = inserted;
        }

        if (shouldDispatchLoaded) EventCarrier(EventBus::Global).Send(PluginLoadedEvent(weakPlugin, meta));
        else TBX_TRACE_VERBOSE("PluginLibraryManager: Duplicate registration detected for '{}'.", meta.Name);
    }

    void PluginManager::EnsureInitialized()
    {
        if (State.Initialized) return;

        State.Listener.Bind(EventBus::Global);
        State.Listener.Listen<PluginDestroyedEvent>(&PluginManager::HandleDestroyed);
        State.Initialized = true;
    }

    void PluginManager::HandleDestroyed(PluginDestroyedEvent& event)
    {
        LoadedPlugin unloaded = {};
        if (!RemovePlugin(event.Plugin, unloaded))
        {
            TBX_TRACE_WARNING("PluginLibraryManager: Received destruction for untracked plugin at {}.", static_cast<const void*>(event.Plugin));
            return;
        }

        EventCarrier(EventBus::Global).Send(PluginUnloadedEvent(unloaded.Instance, unloaded.Meta));
    }

    void PluginManager::Unregister(const Plugin* plugin)
    {
        EnsureInitialized();
        if (!plugin)
        {
            TBX_TRACE_WARNING("PluginLibraryManager: Ignoring manual unregister for null plugin pointer.");
            return;
        }

        LoadedPlugin unloaded = {};
        if (!RemovePlugin(plugin, unloaded))
        {
            TBX_TRACE_WARNING("PluginLibraryManager: Manual unregister requested for untracked plugin at {}.", static_cast<const void*>(plugin));
            return;
        }

        TBX_ASSERT(unloaded.Instance.lock().use_count() != 1, "PluginLibraryManager: Dangling references detected from manually unregistering plugin '{}'; .", unloaded.Meta.Name);
        EventCarrier(EventBus::Global).Send(PluginUnloadedEvent(unloaded.Instance, unloaded.Meta));
    }
}
