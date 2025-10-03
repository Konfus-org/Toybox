#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginLoader.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Debug/ILogger.h"
#include <algorithm>
#include <memory>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace Tbx
{
    namespace PluginLoaderDetail
    {
        bool ArePluginDependenciesSatisfied(
            const PluginMeta& info,
            const std::unordered_set<std::string>& loadedNames)
        {
            for (const auto& dep : info.Dependencies)
            {
                if (dep == "All")
                {
                    continue;
                }

                if (loadedNames.contains(dep))
                {
                    continue;
                }

                return false;
            }

            return true;
        }

        void ReportUnresolvedPluginDependencies(
            const std::vector<PluginMeta>& remaining,
            const std::unordered_set<std::string>& loadedNames)
        {
            std::ostringstream oss = {};
            oss << "Unresolved plugin dependencies:\n";

            for (const auto& info : remaining)
            {
                std::vector<std::string> missing;
                for (const auto& dep : info.Dependencies)
                {
                    if (!loadedNames.contains(dep) && dep != "All")
                    {
                        missing.push_back(dep);
                    }
                }

                oss << "  - " << info.Name;
                if (!missing.empty())
                {
                    oss << " (missing: ";
                    for (size_t i = 0; i < missing.size(); ++i)
                    {
                        if (i != 0)
                        {
                            oss << ", ";
                        }
                        oss << missing[i];
                    }
                    oss << ")";
                }
                oss << "\n";
            }

            TBX_TRACE_ERROR("PluginLoader: {}", oss.str());
        }

        void ReportPluginInfo(const PluginMeta& info)
        {
            TBX_TRACE_INFO("- Loaded {}:", info.Name);
            TBX_TRACE_INFO("    - Version: {}", info.Version);
            TBX_TRACE_INFO("    - Author: {}", info.Author);
            TBX_TRACE_INFO("    - Description: {}", info.Description);
        }

        bool LoadPlugin(
            const PluginMeta& info,
            Ref<EventBus> eventBus,
            std::unordered_set<std::string>& loadedNames,
            std::vector<Ref<Plugin>>& outLoaded)
        {
            if (info.IsStatic)
            {
                TBX_TRACE_ERROR("PluginLoader: Static plugins are not supported for '{}'", info.Name);
                return false;
            }

            SharedLibrary library;
            if (!library.Load(info.Path))
            {
                TBX_TRACE_ERROR("PluginLoader: Failed to load library at '{}'", info.Path);
                return false;
            }

            const auto loadFuncSymbol = library.GetSymbol(TBX_LOAD_PLUGIN_FN_NAME);
            const auto unloadFuncSymbol = library.GetSymbol(TBX_UNLOAD_PLUGIN_FN_NAME);
            const auto loadPluginFunc = reinterpret_cast<PluginLoadFn>(loadFuncSymbol);
            const auto unloadPluginFunc = reinterpret_cast<PluginUnloadFn>(unloadFuncSymbol);
            if (loadPluginFunc == nullptr)
            {
                TBX_TRACE_ERROR(
                    "PluginLoader: Missing load function in '{}'. Did it call TBX_REGISTER_PLUGIN?",
                    info.Name);
                library.Unload();
                return false;
            }

            if (unloadPluginFunc == nullptr)
            {
                TBX_TRACE_ERROR(
                    "PluginLoader: Missing unload function in '{}'. Did it call TBX_REGISTER_PLUGIN?",
                    info.Name);
                library.Unload();
                return false;
            }

            Plugin* pluginInstance = loadPluginFunc(eventBus);
            if (pluginInstance == nullptr)
            {
                TBX_TRACE_ERROR("PluginLoader: Load returned nullptr for '{}'", info.Name);
                library.Unload();
                return false;
            }

            pluginInstance->Initialize(info, library);
            if (!pluginInstance->IsInitialized())
            {
                TBX_TRACE_ERROR("PluginLoader: Plugin '{}' failed to initialize", info.Name);
                unloadPluginFunc(pluginInstance);
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

#ifdef TBX_VERBOSE_LOGGING
            if (plugin->HasLibrary())
            {
                plugin->GetLibrary().ListSymbols();
            }
#endif

            if (eventBus != nullptr)
            {
                eventBus->Post(PluginLoadedEvent(plugin));
            }

            ReportPluginInfo(info);
            loadedNames.insert(info.Name);
            outLoaded.push_back(std::move(plugin));
            return true;
        }

        bool SortKey(const PluginMeta& a, const PluginMeta& b)
        {
            const auto dependenciesA = a.Dependencies.size();
            const auto dependenciesB = b.Dependencies.size();
            if (dependenciesA != dependenciesB)
            {
                return dependenciesA < dependenciesB;
            }

            return a.Name < b.Name;
        }
    }

    PluginCollection::PluginCollection(std::vector<Ref<Plugin>> plugins, Ref<EventBus> eventBus)
        : _data(std::make_shared<PluginCollectionData>())
    {
        _data->plugins = std::move(plugins);
        _data->eventBus = std::move(eventBus);
    }

    PluginCollection::~PluginCollection()
    {
        if (_data == nullptr)
        {
            return;
        }

        if (!_data.unique())
        {
            return;
        }

        auto& plugins = _data->plugins;
        if (plugins.empty())
        {
            return;
        }

        std::vector<Ref<Plugin>> pluginSnapshot;
        if (_data->eventBus != nullptr)
        {
            pluginSnapshot.reserve(plugins.size());
            for (const auto& plugin : plugins)
            {
                if (plugin != nullptr)
                {
                    pluginSnapshot.push_back(plugin);
                }
            }
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

            TBX_TRACE_INFO("PluginCollection: Releasing {}", plugin->GetMeta().Name);
            if (plugin.use_count() > 1)
            {
                TBX_ASSERT(
                    false,
                    "{} Plugin is still in use! Ensure all references are released before shutting down!",
                    plugin->GetMeta().Name);
            }

            if (_data->eventBus != nullptr)
            {
                _data->eventBus->Send(PluginUnloadedEvent(plugin));
            }
        }

        if (_data->eventBus != nullptr)
        {
            _data->eventBus->Send(PluginsUnloadedEvent(pluginSnapshot));
        }
    }

    bool PluginCollection::Empty() const
    {
        return Items().empty();
    }

    uint32 PluginCollection::Count() const
    {
        return static_cast<uint32>(Items().size());
    }

    std::vector<Ref<Plugin>> PluginCollection::All() const
    {
        std::vector<Ref<Plugin>> result;
        const auto& plugins = Items();
        result.reserve(plugins.size());

        for (const auto& plugin : plugins)
        {
            if (plugin != nullptr)
            {
                result.push_back(plugin);
            }
        }

        return result;
    }

    Ref<Plugin> PluginCollection::OfName(const std::string& pluginName) const
    {
        const auto& plugins = Items();
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

    const std::vector<Ref<Plugin>>& PluginCollection::Items() const
    {
        static const std::vector<Ref<Plugin>> empty = {};
        if (_data == nullptr)
        {
            return empty;
        }

        return _data->plugins;
    }

    PluginLoader::PluginLoader(
        std::vector<PluginMeta> pluginMetas,
        Ref<EventBus> eventBus)
        : _eventBus(std::move(eventBus))
    {
        LoadPlugins(std::move(pluginMetas));
    }

    PluginCollection PluginLoader::Results()
    {
        return PluginCollection(std::move(_plugins), _eventBus);
    }

    void PluginLoader::LoadPlugins(std::vector<PluginMeta> pluginMetas)
    {
        using namespace PluginLoaderDetail;

        std::stable_sort(pluginMetas.begin(), pluginMetas.end(), SortKey);

        TBX_TRACE_INFO("PluginLoader: Loading plugins:");
        auto loadedNames = std::unordered_set<std::string>();
        uint32 successfullyLoaded = 0;
        uint32 unsuccessfullyLoaded = 0;

        while (!pluginMetas.empty())
        {
            bool resolvedDependencies = false;

            for (auto it = pluginMetas.begin(); it != pluginMetas.end();)
            {
                if (ArePluginDependenciesSatisfied(*it, loadedNames))
                {
                    if (LoadPlugin(*it, _eventBus, loadedNames, _plugins))
                    {
                        ++successfullyLoaded;
                    }
                    else
                    {
                        ++unsuccessfullyLoaded;
                    }

                    it = pluginMetas.erase(it);
                    resolvedDependencies = true;
                }
                else
                {
                    ++it;
                }
            }

            if (!resolvedDependencies)
            {
                TBX_ASSERT(false, "PluginLoader: Unable to resolve some plugin dependencies!");
                ReportUnresolvedPluginDependencies(pluginMetas, loadedNames);
                break;
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
