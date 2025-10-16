#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginLoader.h"
#include "Tbx/Events/EventCarrier.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Memory/Refs.h"
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
        TBX_TRACE_INFO("    - Description: {}", info.Description);
    }

    static bool LoadPlugin(
        const PluginMeta& info,
        Ref<EventBus> eventBus,
        std::unordered_set<std::string>& loadedNames,
        std::vector<Ref<Plugin>>& outLoaded)
    {
        auto library = MakeExclusive<SharedLibrary>();
        if (!library->Load(info.Path))
        {
            TBX_TRACE_ERROR("PluginLoader: Failed to load library at '{}'", info.Path);
            return false;
        }

        const auto loadFuncSymbol = library->GetSymbol(TBX_LOAD_PLUGIN_FN_NAME);
        const auto unloadFuncSymbol = library->GetSymbol(TBX_UNLOAD_PLUGIN_FN_NAME);
        const auto loadPluginFunc = reinterpret_cast<PluginLoadFn>(loadFuncSymbol);
        const auto unloadPluginFunc = reinterpret_cast<PluginUnloadFn>(unloadFuncSymbol);
        if (loadPluginFunc == nullptr)
        {
            TBX_TRACE_ERROR(
                "PluginLoader: Missing load function in '{}'. Did it call TBX_REGISTER_PLUGIN?",
                info.Name);
            library->Unload();
            return false;
        }

        if (unloadPluginFunc == nullptr)
        {
            TBX_TRACE_ERROR(
                "PluginLoader: Missing unload function in '{}'. Did it call TBX_REGISTER_PLUGIN?",
                info.Name);
            library->Unload();
            return false;
        }

        Plugin* pluginInstance = loadPluginFunc(eventBus);
        if (pluginInstance == nullptr)
        {
            TBX_TRACE_ERROR("PluginLoader: Load returned nullptr for '{}'", info.Name);
            library->Unload();
            return false;
        }

        Ref<Plugin> plugin(pluginInstance, [unloadPluginFunc](Plugin* pluginToUnload)
        {
            if (pluginToUnload == nullptr)
            {
                return;
            }

            unloadPluginFunc(pluginToUnload);
        });

        plugin->Meta = info;
        plugin->Library = std::move(library);
        if (!plugin->Library)
        {
            TBX_TRACE_ERROR("PluginLoader: Plugin '{}' failed to initialize", info.Name);
            plugin.reset();
            return false;
        }

#ifdef TBX_VERBOSE_LOGGING
        plugin->ListSymbols();
#endif

        ReportPluginInfo(info);
        loadedNames.insert(info.Name);
        outLoaded.push_back(plugin);
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
        TBX_TRACE_INFO("PluginLoader: Loading plugins:");
        auto loadedNames = std::unordered_set<std::string>();
        uint32 successfullyLoaded = 0;
        uint32 unsuccessfullyLoaded = 0;

        for (const auto& meta : pluginMetas)
        {
            if (LoadPlugin(meta, _eventBus, loadedNames, _plugins))
            {
                ++successfullyLoaded;
            }
            else
            {
                ++unsuccessfullyLoaded;
            }
        }

        TBX_TRACE_INFO("PluginLoader: Successfully loaded {} plugins!", successfullyLoaded);
        TBX_TRACE_INFO("PluginLoader: Failed to load {} plugins!\n", unsuccessfullyLoaded);
    }
}
