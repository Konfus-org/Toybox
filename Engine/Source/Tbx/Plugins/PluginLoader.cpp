#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginLoader.h"
#include "Tbx/Events/EventCarrier.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Plugins/PluginManager.h"
#include <memory>
#include <unordered_set>
#include <utility>

namespace Tbx
{
    static void ReportPluginInfo(const PluginMeta& info)
    {
        TBX_TRACE_INFO("- Loaded {}:", info.Name);
        TBX_TRACE_INFO("    - Version: {}", info.Version);
        TBX_TRACE_INFO("    - Author: {}", info.Author);
        TBX_TRACE_INFO("    - Description: {}\n", info.Description);
    }

    static bool LoadPlugin(
        const PluginMeta& info,
        Ref<EventBus> eventBus,
        std::unordered_set<std::string>& loadedNames,
        std::vector<Ref<Plugin>>& outLoaded)
    {
        auto library = MakeExclusive<SharedLibrary>(info.Path);
        if (!library->IsValid())
        {
            TBX_TRACE_ERROR("PluginLoader: Failed to load library at '{}'", info.Path);
            return false;
        }

        const auto pluginName = info.Name;
        const auto loadFuncSymbol = library->GetSymbol(TBX_LOAD_PLUGIN_FN_NAME);
        const auto unloadFuncSymbol = library->GetSymbol(TBX_UNLOAD_PLUGIN_FN_NAME);
        const auto loadPluginFunc = reinterpret_cast<PluginLoadFn>(loadFuncSymbol);
        const auto unloadPluginFunc = reinterpret_cast<PluginUnloadFn>(unloadFuncSymbol);
        if (loadPluginFunc == nullptr)
        {
            TBX_TRACE_ERROR(
                "PluginLoader: Missing load function in '{}'. Did it call TBX_REGISTER_PLUGIN?",
                info.Name);
            library.reset();
            return false;
        }

        if (unloadPluginFunc == nullptr)
        {
            TBX_TRACE_ERROR(
                "PluginLoader: Missing unload function in '{}'. Did it call TBX_REGISTER_PLUGIN?",
                pluginName);
            library.reset();
            return false;
        }

        Plugin* pluginInstance = loadPluginFunc(eventBus);
        if (pluginInstance == nullptr)
        {
            TBX_TRACE_ERROR("PluginLoader: Load returned nullptr for '{}'", pluginName);
            library.reset();
            return false;
        }

        auto plugin = Ref<Plugin>(pluginInstance, [unloadPluginFunc, pluginName](Plugin* pluginToUnload)
        {
            TBX_TRACE_INFO("Plugin: Unloaded {}\n", pluginName);
            EventCarrier(EventBus::Global).Send(PluginDestroyedEvent(pluginToUnload));
            unloadPluginFunc(pluginToUnload);
        });

#ifdef TBX_VERBOSE_LOGGING
        plugin->ListSymbols();
#endif

        loadedNames.insert(pluginName);
        outLoaded.push_back(plugin);
        ReportPluginInfo(info);
        PluginManager::Register(plugin, info, library);

        return true;
    }

    PluginLoader::PluginLoader(
        const std::vector<PluginMeta>& pluginMetas,
        Ref<EventBus> eventBus)
        : _eventBus(eventBus)
    {
        LoadPlugins(pluginMetas);
    }

    Queryable<Ref<Plugin>> PluginLoader::Results()
    {
        return Queryable<Ref<Plugin>>(_plugins);
    }

    void PluginLoader::LoadPlugins(const std::vector<PluginMeta>& pluginMetas)
    {
        TBX_TRACE_INFO("PluginLoader: Loading plugins...\n");
        auto loadedNames = std::unordered_set<std::string>();
        uint32 successfullyLoaded = 0;
        uint32 unsuccessfullyLoaded = 0;

        for (const auto& meta : pluginMetas)
        {
            if (LoadPlugin(meta, _eventBus, loadedNames, _plugins)) successfullyLoaded++;
            else unsuccessfullyLoaded++;
        }

        TBX_TRACE_INFO("PluginLoader: Successfully loaded {} plugins!", successfullyLoaded);
        TBX_TRACE_INFO("PluginLoader: Failed to load {} plugins!\n", unsuccessfullyLoaded);
    }
}
