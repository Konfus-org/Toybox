#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginLoader.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Debug/ILogger.h"
#include "Tbx/Memory/Refs.h"
#include <algorithm>
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

        plugin->Bind(info, std::move(library));
        if (!plugin->IsBound())
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
        outLoaded.push_back(std::move(plugin));
        return true;
    }

    PluginContainer::PluginContainer(const std::vector<Ref<Plugin>>& plugins)
        : Queryable<Ref<Plugin>>(plugins)
    {
    }

    PluginContainer::~PluginContainer()
    {
        auto& plugins = this->MutableItems();
        if (plugins.empty())
        {
            return;
        }

        std::vector<Ref<Plugin>> pluginSnapshot;
        pluginSnapshot.reserve(plugins.size());
        Ref<EventBus> eventBus = nullptr;
        for (const auto& plugin : plugins)
        {
            if (plugin == nullptr)
            {
                continue;
            }

            pluginSnapshot.push_back(plugin);
        }

        std::stable_partition(
            plugins.begin(),
            plugins.end(),
            [](const Ref<Plugin>& plugin)
            {
                if (plugin == nullptr)
                {
                    return false;
                }

                return std::dynamic_pointer_cast<ILogger>(plugin) != nullptr;
            });

        while (!plugins.empty())
        {
            auto plugin = std::move(plugins.back());
            plugins.pop_back();

            if (plugin == nullptr)
            {
                continue;
            }

            TBX_TRACE_INFO("PluginCache: Releasing {}", plugin->GetMeta().Name);
            if (plugin.use_count() > 1)
            {
                TBX_ASSERT(
                    false,
                    "{} Plugin is still in use! Ensure all references are released before shutting down!",
                    plugin->GetMeta().Name);
            }
        }
    }

    Ref<Plugin> PluginContainer::OfName(const std::string& pluginName) const
    {
        const auto& plugins = this->All();
        const auto it = std::find_if(
            plugins.begin(),
            plugins.end(),
            [&pluginName](const Ref<Plugin>& plugin)
            {
                return plugin != nullptr && plugin->GetMeta().Name == pluginName;
            });

        if (it != plugins.end())
        {
            return *it;
        }

        return nullptr;
    }

    PluginLoader::PluginLoader(
        std::vector<PluginMeta> pluginMetas,
        Ref<EventBus> eventBus)
        : _eventBus(std::move(eventBus))
    {
        LoadPlugins(std::move(pluginMetas));
    }

    PluginContainer PluginLoader::Results()
    {
        return PluginContainer(std::move(_plugins));
    }

    void PluginLoader::LoadPlugins(std::vector<PluginMeta> pluginMetas)
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

        if (_eventBus != nullptr)
        {
            _eventBus->Post(PluginsLoadedEvent(_plugins));
        }
    }
}
