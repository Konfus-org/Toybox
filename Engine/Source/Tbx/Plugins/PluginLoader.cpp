#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginLoader.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Debug/ILogger.h"
#include <algorithm>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace
{
    bool ArePluginDependenciesSatisfied(
        const Tbx::PluginMeta& info,
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
        const std::vector<Tbx::PluginMeta>& remaining,
        const std::unordered_set<std::string>& loadedNames)
    {
        std::ostringstream oss = {};
        oss << "Unresolved plugin dependencies:\n";
        for (const auto& pi : remaining)
        {
            std::vector<std::string> missing;
            for (const auto& dep : pi.Dependencies)
            {
                if (!loadedNames.contains(dep) && dep != "All")
                {
                    missing.push_back(dep);
                }
            }
            oss << "  - " << pi.Name;
            if (!missing.empty())
            {
                oss << " (missing: ";
                for (size_t i = 0; i < missing.size(); ++i)
                {
                    if (i) oss << ", ";
                    oss << missing[i];
                }
                oss << ")";
            }
            oss << "\n";
        }
        TBX_TRACE_ERROR("PluginLoader: {}", oss.str());
    }

    void ReportPluginInfo(const Tbx::PluginMeta& info)
    {
        const auto& pluginName = info.Name;
        const auto& pluginVersion = info.Version;
        const auto& pluginAuthor = info.Author;
        const auto& pluginDescription = info.Description;

        TBX_TRACE_INFO("- Loaded {0}:", pluginName);
        TBX_TRACE_INFO("    - Version: {0}", pluginVersion);
        TBX_TRACE_INFO("    - Author: {0}", pluginAuthor);
        TBX_TRACE_INFO("    - Description: {0}", pluginDescription);
    }

    std::vector<Tbx::Ref<Tbx::IPlugin>> CollectPluginRefs(
        const std::vector<Tbx::ExclusiveRef<Tbx::LoadedPlugin>>& records)
    {
        std::vector<Ref<IPlugin>> plugins = {};
        plugins.reserve(records.size());

        for (const auto& record : records)
        {
            if (record != nullptr)
            {
                plugins.push_back(record->Get());
            }
        }

        return plugins;
    }

    bool LoadPlugin(
        const Tbx::PluginMeta& info,
        Tbx::Ref<Tbx::EventBus> eventBus,
        std::unordered_set<std::string>& outLoadedPluginNames,
        std::vector<Tbx::ExclusiveRef<Tbx::LoadedPlugin>>& outLoaded)
    {
        auto loadedPlugin = Tbx::MakeExclusive<Tbx::LoadedPlugin>(info, eventBus);
        if (!loadedPlugin || !loadedPlugin->IsValid())
        {
            TBX_ASSERT(false, "PluginLoader: Failed to load plugin: {0}", info.Name);
            return false;
        }

        const auto plugin = loadedPlugin->Get();
        if (eventBus != nullptr)
        {
            eventBus->Post(Tbx::PluginLoadedEvent(plugin));
        }
        ReportPluginInfo(info);

        outLoadedPluginNames.insert(loadedPlugin->GetMeta().Name);
        outLoaded.push_back(std::move(loadedPlugin));

        return true;
    }

    bool SortKey(const Tbx::PluginMeta& a, const Tbx::PluginMeta& b)
    {
        const auto da = a.Dependencies.size();
        const auto db = b.Dependencies.size();
        if (da != db) return da < db;
        return a.Name < b.Name;
    }
}

namespace Tbx
{
    PluginCollection::PluginCollection(
        std::vector<ExclusiveRef<LoadedPlugin>> plugins,
        Ref<EventBus> eventBus) noexcept
        : _plugins(std::move(plugins))
        , _eventBus(std::move(eventBus))
    {
    }

    PluginCollection::~PluginCollection()
    {
        if (_plugins.empty())
        {
            return;
        }

        std::vector<Ref<IPlugin>> pluginsView;
        if (_eventBus != nullptr)
        {
            pluginsView.reserve(_plugins.size());
            for (const auto& record : _plugins)
            {
                if (record != nullptr)
                {
                    pluginsView.push_back(record->Get());
                }
            }
        }

        std::stable_partition(
            _plugins.begin(),
            _plugins.end(),
            [](const ExclusiveRef<LoadedPlugin>& record)
            {
                return record != nullptr && record->GetAs<ILogger>() != nullptr;
            });

        while (!_plugins.empty())
        {
            auto pluginRecord = std::move(_plugins.back());
            _plugins.pop_back();

            if (pluginRecord == nullptr)
            {
                continue;
            }

            TBX_TRACE_INFO("PluginCollection: Releasing {}", pluginRecord->GetMeta().Name);
            auto plugin = pluginRecord->Get();
            if (_eventBus != nullptr)
            {
                _eventBus->Send(PluginUnloadedEvent(plugin));
            }

            auto expectedUseCount = 2;
            if (pluginRecord->GetAs<ILogger>())
            {
                expectedUseCount = 3;
            }
            if (plugin.use_count() > expectedUseCount)
            {
                TBX_ASSERT(
                    false,
                    "{} Plugin is still in use! Ensure all references are released before shutting down!",
                    pluginRecord->GetMeta().Name);
            }
        }

        if (_eventBus != nullptr)
        {
            _eventBus->Send(PluginsUnloadedEvent(pluginsView));
        }
    }

    bool PluginCollection::Empty() const noexcept
    {
        return _plugins.empty();
    }

    uint32 PluginCollection::Count() const noexcept
    {
        return static_cast<uint32>(_plugins.size());
    }

    std::vector<Ref<IPlugin>> PluginCollection::All() const
    {
        std::vector<Ref<IPlugin>> result;
        result.reserve(_plugins.size());

        for (const auto& record : _plugins)
        {
            if (record != nullptr)
            {
                result.push_back(record->Get());
            }
        }

        return result;
    }

    Ref<IPlugin> PluginCollection::OfName(const std::string& pluginName) const
    {
        const auto it = std::find_if(
            _plugins.begin(),
            _plugins.end(),
            [&pluginName](const ExclusiveRef<LoadedPlugin>& record)
            {
                return record != nullptr && record->GetMeta().Name == pluginName;
            });

        if (it != _plugins.end())
        {
            return (*it)->Get();
        }

        return nullptr;
    }

    //////////// PLUGIN MANAGER //////////////////

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
        std::stable_sort(pluginMetas.begin(), pluginMetas.end(), SortKey);

        TBX_TRACE_INFO("PluginLoader: Loading plugins:");
        auto loadedNames = std::unordered_set<std::string>();
        uint pluginsSuccessfullyLoaded = 0;
        uint pluginsUnsuccessfullyLoaded = 0;
        while (!pluginMetas.empty())
        {
            bool resolvedDeps = false;

            for (auto it = pluginMetas.begin(); it != pluginMetas.end(); )
            {
                if (ArePluginDependenciesSatisfied(*it, loadedNames))
                {
                    if (LoadPlugin(*it, _eventBus, loadedNames, _plugins))
                    {
                        pluginsSuccessfullyLoaded++;
                    }
                    else
                    {
                        pluginsUnsuccessfullyLoaded++;
                    }
                    it = pluginMetas.erase(it);
                    resolvedDeps = true;
                }
                else
                {
                    ++it;
                }
            }

            if (!resolvedDeps)
            {
                TBX_ASSERT(false, "PluginLoader: Unable to resolve some plugin dependencies!");
                ReportUnresolvedPluginDependencies(pluginMetas, loadedNames);
                break;
            }
        }

        TBX_TRACE_INFO("PluginLoader: Successfully loaded {} plugins!", pluginsSuccessfullyLoaded);
        TBX_TRACE_INFO("PluginLoader: Failed to load {} plugins!\n", pluginsUnsuccessfullyLoaded);

        if (_eventBus != nullptr)
        {
            _eventBus->Post(PluginsLoadedEvent(CollectPluginRefs(_plugins)));
        }
    }
}
