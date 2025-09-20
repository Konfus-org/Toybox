#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginServer.h"
#include "Tbx/Plugins/LoadedPlugin.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Debug/Debugging.h"
#include <algorithm>
#include <unordered_set>
#include <sstream>

namespace Tbx
{
	//////////// UTILS //////////////////

	/// <summary>
	/// Returns true if the plugin implements the given type.
	/// </summary>
	static bool DoesPluginImplementType(const PluginMeta& info, std::string_view type)
	{
		const auto& types = info.GetImplementedTypes();
		return std::find(types.begin(), types.end(), type) != types.end();
	}

	/// <summary>
	/// Dependencies satisfied if each dep is either:
	///  - the name of a loaded plugin, OR
	///  - a type that at least one loaded plugin implements.
	/// </summary>
	static bool ArePluginDependenciesSatisfied(
		const PluginMeta& info,
		const std::unordered_set<std::string>& loadedNames,
		const std::unordered_set<std::string>& loadedTypes)
	{
		for (const auto& dep : info.GetDependencies())
		{
			if (dep == "All")
			{
				continue;
			}
			if (loadedNames.count(dep) || loadedTypes.count(dep))
			{
				continue;
			}
			return false;
		}
		return true;
	}

	/// <summary>
	/// Helpful error text when dependency resolution stalls.
	/// </summary>
	static void ReportUnresolvedPluginDependencies(
		const std::vector<PluginMeta>& remaining,
		const std::unordered_set<std::string>& loadedNames,
		const std::unordered_set<std::string>& loadedTypes)
	{
		std::ostringstream oss;
		oss << "Unresolved plugin dependencies:\n";
		for (const auto& pi : remaining)
		{
			std::vector<std::string> missing;
			for (const auto& dep : pi.GetDependencies())
			{
				if (!loadedNames.count(dep) && !loadedTypes.count(dep))
				{
					missing.push_back(dep);
				}
			}
			oss << "  - " << pi.GetName();
			if (!missing.empty())
			{
				oss << " (missing: ";
				for (size_t i = 0; i < missing.size(); ++i) {
					if (i) oss << ", ";
					{
						oss << missing[i];
					}
				}
				oss << ")";
			}
			oss << "\n";
		}
		TBX_TRACE_ERROR("{}", oss.str());
	}

	/// <summary>
	/// Reports info on loaded plugin.
	/// </summary>
	static void ReportPluginInfo(const PluginMeta& info)
	{
		const auto& pluginName = info.GetName();
		const auto& pluginVersion = info.GetVersion();
		const auto& pluginAuthor = info.GetAuthor();
		const auto& pluginDescription = info.GetDescription();

		TBX_TRACE_INFO("{0}:", pluginName);
		TBX_TRACE_INFO("    - Version: {0}", pluginVersion);
		TBX_TRACE_INFO("    - Author: {0}", pluginAuthor);
		TBX_TRACE_INFO("    - Description: {0}", pluginDescription);
	}

	/// <summary>
	/// Load one plugin, update name/type sets, emit event.
	/// </summary>
	static bool LoadPlugin(
		const PluginMeta& info,
		WeakRef<App> app,
		Ref<EventBus> eventBus,
		std::unordered_set<std::string>& loadedNames,
		std::unordered_set<std::string>& loadedTypes,
		std::vector<Ref<LoadedPlugin>>& outLoaded)
	{
		auto plugin = std::make_shared<LoadedPlugin>(info, app);
		if (!plugin->IsValid())
		{
			TBX_ASSERT(false, "Failed to load plugin: {0}", info.GetName());
			return false;
		}

		outLoaded.push_back(plugin);
		loadedNames.insert(plugin->GetMeta().GetName());
		for (const auto& t : plugin->GetMeta().GetImplementedTypes())
		{
			loadedTypes.insert(t);
		}

		eventBus->Post(PluginLoadedEvent(plugin));
		ReportPluginInfo(info);

		return true;
	}

	/// <summary>Sort key: fewer dependencies first, then by name for stability.</summary>
	static bool SortKey(const PluginMeta& a, const PluginMeta& b)
	{
		const auto da = a.GetDependencies().size();
		const auto db = b.GetDependencies().size();
		if (da != db) return da < db;
		return a.GetName() < b.GetName();
	}

	//////////// PLUGIN MANAGER //////////////////

	PluginServer::PluginServer(
		const std::string& pathToPlugins,
		Ref<EventBus> eventBus,
		WeakRef<Tbx::App> app)
	{
		_eventBus = eventBus;
		LoadPlugins(pathToPlugins, app);
	}

	PluginServer::~PluginServer()
	{
		UnloadPlugins();
	}

	void PluginServer::AddPlugin(const Ref<LoadedPlugin>& plugin)
	{
		_loadedPlugins.push_back(plugin);
	}

	const std::vector<Ref<LoadedPlugin>>& PluginServer::GetPlugins()
	{
		return _loadedPlugins;
	}

	std::vector<PluginMeta> PluginServer::SearchDirectoryForInfos(const std::string& pathToPlugins)
	{
		std::vector<PluginMeta> foundPluginInfos = {};

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

				auto plugInfo = PluginMeta(entry.path().parent_path().string(), fileName);
				TBX_ASSERT(plugInfo.IsValid(), "Invalid plugin info at: {0}!", entry.path().string());
				if (!plugInfo.IsValid()) continue;

				foundPluginInfos.push_back(plugInfo);
			}
		}

		return foundPluginInfos;
	}

	void PluginServer::LoadPlugins(const std::string& pathToPlugins, std::weak_ptr<Tbx::App> app)
	{
		// 1) Discover all plugin infos
		auto allInfos = SearchDirectoryForInfos(pathToPlugins);

		// 2) Partition: "All" plugins are deferred to the end
		std::vector<PluginMeta> normal, allLast;
		normal.reserve(allInfos.size());
		for (auto& pi : allInfos)
		{
			if (DoesPluginImplementType(pi, "All"))
			{
				allLast.push_back(pi);
			}
			else
			{
				normal.push_back(pi);
			}
		}

		// Stable, nice-to-read order inside each bucket
		std::stable_sort(normal.begin(), normal.end(), SortKey);
		std::stable_sort(allLast.begin(), allLast.end(), SortKey);

		// 3) Resolve a bucket with dependency scheduling
		auto loadedNames = std::unordered_set<std::string>();
		auto loadedTypes = std::unordered_set<std::string>();
		auto resolveBucket = [&](std::vector<PluginMeta>& bucket)
		{
			while (!bucket.empty())
			{
				bool progress = false;

				// Iterate and grab everything that’s ready this round
				for (auto it = bucket.begin(); it != bucket.end(); )
				{
					if (ArePluginDependenciesSatisfied(*it, loadedNames, loadedTypes))
					{
						LoadPlugin(*it, app, _eventBus, loadedNames, loadedTypes, _loadedPlugins);
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
					ReportUnresolvedPluginDependencies(bucket, loadedNames, loadedTypes);
					TBX_ASSERT(false, "Unable to resolve plugin dependencies!");
					break;
				}
			}
		};

		// 5) Load “normal” first, then the ones explicitly marked to wait until the end
		resolveBucket(normal);
		resolveBucket(allLast);
	}

	void PluginServer::UnloadPlugins()
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
				TBX_ASSERT(false, "{} Plugin is still in use! Ensure all references are released before shutting down!", plugin->GetMeta().GetName());
			}

			TBX_TRACE_INFO("Unloading plugin: {}", plugin->GetMeta().GetName());
			plugin.reset(); // plugin unloads itself
		}
	}
}