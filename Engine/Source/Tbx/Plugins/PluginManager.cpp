#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginManager.h"
#include "Tbx/Plugins/LoadedPlugin.h"
#include "Tbx/Plugins/PluginUtils.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
	PluginManager::PluginManager(const std::string& pathToPlugins, std::shared_ptr<EventBus> eventBus)
	{
		_eventBus = eventBus;
		LoadPlugins(pathToPlugins);
	}

	PluginManager::~PluginManager()
	{
		UnloadPlugins();
	}

	void PluginManager::AddPlugin(const std::shared_ptr<LoadedPlugin>& plugin)
	{
		_loadedPlugins.push_back(plugin);
	}

	const std::vector<std::shared_ptr<LoadedPlugin>>& PluginManager::GetPlugins()
	{
		return _loadedPlugins;
	}

	std::vector<PluginInfo> PluginManager::SearchDirectoryForInfos(const std::string& pathToPlugins)
	{
		std::vector<PluginInfo> foundPluginInfos = {};

		const auto& filesInPluginDir = std::filesystem::directory_iterator(pathToPlugins);
		for (const auto& entry : filesInPluginDir)
		{
			// Recursively search directories
			if (entry.is_directory())
			{
				auto pluginInfosFoundInDir = SearchDirectoryForInfos(entry.path().string());
				for (const auto& pluginInfo : pluginInfosFoundInDir)
				{
					foundPluginInfos.push_back(pluginInfo);
				}
			}

			// Skip anything that isn't a file
			if (!entry.is_regular_file()) continue;

			// Extension check
			if (entry.path().extension() == ".meta")
			{
				const std::string& fileName = entry.path().filename().string();

				auto plugInfo = PluginInfo(entry.path().parent_path().string(), fileName);
				TBX_ASSERT(plugInfo.IsValid(), "Invalid plugin info at: {0}!", entry.path().string());
				if (!plugInfo.IsValid()) continue;

				foundPluginInfos.push_back(plugInfo);
			}
		}

		return foundPluginInfos;
	}

	void PluginManager::LoadPlugins(const std::string& pathToPlugins)
	{
		// 1) Discover all plugin infos
		auto allInfos = SearchDirectoryForInfos(pathToPlugins);

		// 2) Partition: "All" plugins are deferred to the end
		std::vector<PluginInfo> normal, allLast;
		normal.reserve(allInfos.size());
		for (auto& pi : allInfos)
		{
			if (Plugin::ImplementsType(pi, "All"))
			{
				allLast.push_back(pi);
			}
			else
			{
				normal.push_back(pi);
			}
		}

		// Stable, nice-to-read order inside each bucket
		std::stable_sort(normal.begin(), normal.end(), Plugin::SortKey);
		std::stable_sort(allLast.begin(), allLast.end(), Plugin::SortKey);

		// 3) Resolve a bucket with dependency scheduling
		auto loadedNames = std::unordered_set<std::string>();
		auto loadedTypes = std::unordered_set<std::string>();
		auto resolveBucket = [&](std::vector<PluginInfo>& bucket)
		{
			while (!bucket.empty())
			{
				bool progress = false;

				// Iterate and grab everything that’s ready this round
				for (auto it = bucket.begin(); it != bucket.end(); )
				{
					if (Plugin::AreDependenciesSatisfied(*it, loadedNames, loadedTypes))
					{
						Plugin::Load(*it, _eventBus, loadedNames, loadedTypes, _loadedPlugins);
						it = bucket.erase(it);
						progress = true;
					}
					else
					{
						++it;
					}
				}

				if (!progress)
				{
					Plugin::ReportUnresolvedDependencies(bucket, loadedNames, loadedTypes);
					TBX_ASSERT(false, "Unable to resolve plugin dependencies!");
					break;
				}
			}
		};

		// 5) Load “normal” first, then the ones explicitly marked to wait until the end
		resolveBucket(normal);
		resolveBucket(allLast);
	}

	void PluginManager::UnloadPlugins()
	{
		TBX_TRACE_INFO("Unloading plugins...");

		// Clear refs to loaded plugins.. 
		// this will cause them to unload themselves
		// Unload one by one, reverse load order
		while (!_loadedPlugins.empty())
		{
			auto plugin = std::move(_loadedPlugins.back());
			_loadedPlugins.pop_back();

			const auto refs = plugin->Get().use_count();
			if (refs > 1) // We have one ref above
			{
				TBX_ASSERT(false, "{} Plugin is still in use! Ensure all references are released before shutting down!", plugin->GetInfo().GetName());
			}

			TBX_TRACE_INFO("Unloading plugin: {}", plugin->GetInfo().GetName());
			plugin.reset(); // plugin unloads itself
		}
	}
}