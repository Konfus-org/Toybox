#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginServer.h"
#include "Tbx/Plugins/LoadedPlugin.h"
#include "Tbx/Plugins/PluginMetaReader.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Debug/Debugging.h"
#include <algorithm>
#include <unordered_set>
#include <sstream>

namespace Tbx
{
	//////////// UTILS //////////////////

	/// <summary>
	/// Dependencies satisfied if each dep is either:
	///  - the name of a loaded plugin, OR
	///  - a type that at least one loaded plugin implements.
	/// </summary>
	static bool ArePluginDependenciesSatisfied(
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

	/// <summary>
	/// Helpful error text when dependency resolution stalls.
	/// </summary>
	static void ReportUnresolvedPluginDependencies(
		const std::vector<PluginMeta>& remaining,
		const std::unordered_set<std::string>& loadedNames)
	{
		std::ostringstream oss = {};
		oss << "Unresolved plugin dependencies:\n";
		for (const auto& pi : remaining)
		{
			std::vector<std::string> missing;
			for (const auto& dep : pi.Dependencies)
			{
				if (!loadedNames.contains(dep))
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
		const auto& pluginName = info.Name;
		const auto& pluginVersion = info.Version;
		const auto& pluginAuthor = info.Author;
		const auto& pluginDescription = info.Description;

		TBX_TRACE_INFO("{0}:", pluginName);
		TBX_TRACE_INFO("    - Version: {0}", pluginVersion);
		TBX_TRACE_INFO("    - Author: {0}", pluginAuthor);
		TBX_TRACE_INFO("    - Description: {0}", pluginDescription);
	}

	static PluginMeta LoadPluginMeta(const std::filesystem::path& pathToMeta)
	{
		auto metaData = PluginMetaReader::Read(pathToMeta.string());
		if (metaData.empty()) return {};

		auto meta = PluginMeta();
		meta.Name = metaData["name"][0];
#ifdef TBX_PLATFORM_WINDOWS
		meta.Path = pathToMeta.parent_path().string() + "/" + pathToMeta.filename().stem().string() + ".dll";
#elif
		meta.Path = pathToMeta.parent_path().string() + "/" + pathToMeta.filename().stem().string() + ".so";
#endif
		meta.Author = metaData["author"][0];
		meta.Version = metaData["version"][0];
		meta.Description = metaData["description"][0];
		meta.Dependencies = metaData["dependencies"];
		meta.IsStatic = false;

		return meta;
	}

	/// <summary>
	/// Load one plugin, update name/type sets, emit event.
	/// </summary>
	static bool LoadPlugin(
		const PluginMeta& info,
		WeakRef<App> app,
		Ref<EventBus> eventBus,
		std::unordered_set<std::string>& outLoadedPluginNames,
		std::vector<ExclusiveRef<LoadedPlugin>>& outLoaded)
	{
		auto loadedPlugin = std::make_unique<LoadedPlugin>(info, app);
		if (!loadedPlugin || !loadedPlugin->IsValid())
		{
			TBX_ASSERT(false, "Failed to load plugin: {0}", info.Name);
			return false;
		}

		auto plugin = loadedPlugin->Get();
		eventBus->Post(PluginLoadedEvent(plugin));
		ReportPluginInfo(info);

		outLoadedPluginNames.insert(loadedPlugin->GetMeta().Name);
		outLoaded.push_back(std::move(loadedPlugin));

		return true;
	}

	/// <summary>
	/// Sort key: fewer dependencies first, then by name for stability.
	/// </summary>
	static bool SortKey(const PluginMeta& a, const PluginMeta& b)
	{
		const auto da = a.Dependencies.size();
		const auto db = b.Dependencies.size();
		if (da != db) return da < db;
		return a.Name < b.Name;
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

	void PluginServer::RegisterPlugin(ExclusiveRef<LoadedPlugin> plugin)
	{
		_loadedPlugins.push_back(std::move(plugin));
	}

	std::vector<Ref<Plugin>> PluginServer::GetPlugins() const
	{
		std::vector<Ref<Plugin>> result = {};
		result.reserve(_loadedPlugins.size());
		for (const auto& owned : _loadedPlugins)
		{
			if (auto p = owned->Get())
				result.push_back(std::move(p));
		}
		return result;
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
				auto plugInfo = LoadPluginMeta(entry.path());
				auto pluginInfoValid = !plugInfo.Name.empty();
				TBX_ASSERT(pluginInfoValid, "Invalid plugin info at: {0}!", entry.path().string());
				if (!pluginInfoValid) continue;

				foundPluginInfos.push_back(plugInfo);
			}
		}

		return foundPluginInfos;
	}

	void PluginServer::LoadPlugins(const std::string& pathToPlugins, std::weak_ptr<Tbx::App> app)
	{
		// 1) Discover
		auto infos = SearchDirectoryForInfos(pathToPlugins);

		// 2.) Sort
		std::stable_sort(infos.begin(), infos.end(), SortKey);

		// 3) Load 
		auto loadedNames = std::unordered_set<std::string>();
		while (!infos.empty())
		{
			bool progress = false;

			// Iterate and grab everything that’s ready this round
			for (auto it = infos.begin(); it != infos.end(); )
			{
				if (ArePluginDependenciesSatisfied(*it, loadedNames))
				{
					LoadPlugin(*it, app, _eventBus, loadedNames, _loadedPlugins);
					it = infos.erase(it);
					progress = true;
				}
				else
				{
					++it;
				}
			}

			if (!progress)
			{
				ReportUnresolvedPluginDependencies(infos, loadedNames);
				TBX_ASSERT(false, "Unable to resolve plugin dependencies!");
				break;
			}
		}
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
				TBX_ASSERT(false, "{} Plugin is still in use! Ensure all references are released before shutting down!", plugin->GetMeta().Name);
			}

			TBX_TRACE_INFO("Unloading plugin: {}", plugin->GetMeta().Name);
			plugin.reset(); // plugin unloads itself
		}
	}
}