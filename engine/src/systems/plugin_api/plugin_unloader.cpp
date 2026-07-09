#include "plugin_unloader.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/types/components/component.h"
#include "tbx/utils/string_utils.h"
#include <algorithm>
#include <functional>
#include <optional>

namespace tbx
{
    static bool is_windowing_plugin(const PluginMeta& meta)
    {
        return to_lower(meta.name).find("window") != std::string::npos;
    }

    static bool should_detach_before(const PluginMeta& left, const PluginMeta& right)
    {
        const bool left_is_logging = left.category == PluginCategory::LOGGING;
        const bool right_is_logging = right.category == PluginCategory::LOGGING;
        if (left_is_logging != right_is_logging)
            return !left_is_logging;

        const bool left_is_windowing = is_windowing_plugin(left);
        const bool right_is_windowing = is_windowing_plugin(right);
        const bool left_is_rendering = left.category == PluginCategory::RENDERING;
        const bool right_is_rendering = right.category == PluginCategory::RENDERING;
        if (left_is_rendering && right_is_windowing)
            return true;
        if (right_is_rendering && left_is_windowing)
            return false;

        if (left.priority != right.priority)
            return left.priority > right.priority;

        return to_lower(left.name) < to_lower(right.name);
    }

    static void clear_plugin_owned_resources(
        Uuid plugin_id,
        ServiceProvider& service_provider,
        PluginOwnershipTracker& ownership_tracker)
    {
        if (!plugin_id.is_valid())
            return;

        const auto owned_resources = ownership_tracker.snapshot_and_clear(plugin_id);

        for (const auto& component_type : owned_resources.component_types)
            unregister_entity_component_type_entry(component_type);

        for (const auto& serializable_type_name : owned_resources.serializable_type_names)
            unregister_serializable_type_entry(serializable_type_name);

        for (const auto& asset_type : owned_resources.asset_types)
            unregister_asset_type_entry(asset_type);

        if (auto asset_manager = service_provider.try_get_service<AssetManager>().lock())
        {
            for (const auto& handle : owned_resources.pinned_asset_handles)
                asset_manager->set_pinned(handle, false);

            for (const auto& directory : owned_resources.asset_directories)
                asset_manager->remove_directory(directory);
        }

        if (auto entity_registry = service_provider.try_get_service<EntityRegistry>().lock())
        {
            for (const auto& entity_id : owned_resources.entity_ids)
                entity_registry->get(entity_id).destroy();
        }

        for (const auto& service_type : owned_resources.service_types)
            service_provider.deregister_service(service_type);
    }

    static void unload_plugins(
        LoadedPlugins& loaded_plugins,
        ServiceProvider& service_provider,
        PluginOwnershipTracker& ownership_tracker,
        std::optional<std::reference_wrapper<IMessageCoordinator>> coordinator);

    static void detach_plugins(
        LoadedPlugins& loaded_plugins,
        ServiceProvider& service_provider,
        std::optional<std::reference_wrapper<IMessageCoordinator>> coordinator)
    {
        auto remaining_plugins = std::vector<LoadedPlugin*>();
        remaining_plugins.reserve(loaded_plugins.size());
        for (auto& plugin : loaded_plugins)
            remaining_plugins.push_back(&plugin);

        while (!remaining_plugins.empty())
        {
            auto name_to_index = std::unordered_map<std::string, size>();
            name_to_index.reserve(remaining_plugins.size());
            for (size index = 0; index < static_cast<size>(remaining_plugins.size()); ++index)
                name_to_index.emplace(to_lower(remaining_plugins[index]->meta.name), index);

            auto dependents_count = std::vector<size>(remaining_plugins.size(), size(0));
            for (size index = 0; index < static_cast<size>(remaining_plugins.size()); ++index)
            {
                for (const std::string& dependency : remaining_plugins[index]->meta.dependencies)
                {
                    const std::string lowered = to_lower(trim(dependency));
                    auto dependency_it = name_to_index.find(lowered);
                    if (dependency_it == name_to_index.end())
                        continue;

                    dependents_count[dependency_it->second] += 1U;
                }
            }

            auto candidates = std::vector<size>();
            candidates.reserve(remaining_plugins.size());
            for (size index = 0; index < static_cast<size>(remaining_plugins.size()); ++index)
            {
                if (dependents_count[index] == 0U)
                    candidates.push_back(index);
            }

            if (candidates.empty())
            {
                TBX_TRACE_WARNING(
                    "Plugin detach dependency cycle detected. Falling back to stack order.");
                candidates.push_back(static_cast<size>(remaining_plugins.size() - 1U));
            }

            std::sort(
                candidates.begin(),
                candidates.end(),
                [&remaining_plugins](size left_index, size right_index)
                {
                    const PluginMeta& left = remaining_plugins[left_index]->meta;
                    const PluginMeta& right = remaining_plugins[right_index]->meta;
                    return should_detach_before(left, right);
                });

            const size selected_index = candidates.front();
            remaining_plugins[selected_index]->detach(service_provider);
            if (coordinator.has_value())
                coordinator->get().flush();

            remaining_plugins.erase(
                remaining_plugins.begin() + static_cast<std::ptrdiff_t>(selected_index));
        }
    }

    void PluginUnloader::detach(LoadedPlugins& loaded_plugins, ServiceProvider& service_provider)
    {
        detach_plugins(loaded_plugins, service_provider, std::nullopt);
    }

    void PluginUnloader::detach(
        LoadedPlugins& loaded_plugins,
        ServiceProvider& service_provider,
        IMessageCoordinator& coordinator)
    {
        detach_plugins(loaded_plugins, service_provider, std::ref(coordinator));
    }

    void PluginUnloader::unload(
        LoadedPlugins& loaded_plugins,
        ServiceProvider& service_provider,
        PluginOwnershipTracker& ownership_tracker)
    {
        unload_plugins(loaded_plugins, service_provider, ownership_tracker, std::nullopt);
    }

    static void unload_plugins(
        LoadedPlugins& loaded_plugins,
        ServiceProvider& service_provider,
        PluginOwnershipTracker& ownership_tracker,
        std::optional<std::reference_wrapper<IMessageCoordinator>> coordinator)
    {
        detach_plugins(loaded_plugins, service_provider, coordinator);
        if (coordinator.has_value())
            coordinator->get().flush();

        for (const auto& plugin : loaded_plugins)
        {
            clear_plugin_owned_resources(plugin.get_id(), service_provider, ownership_tracker);
            if (coordinator.has_value())
                coordinator->get().flush();
        }

        // Destroy every plugin instance while every plugin library is still mapped: a plugin's
        // members can hold the last weak_ptr to a service allocated in another plugin's DLL, and
        // releasing that control block runs the owning DLL's code. Clearing the list directly
        // would free each library in load order — dependencies first — before dependents'
        // instances are destroyed. Dependents load after their dependencies, so destroy
        // back-to-front; the libraries are then freed by clear() once no instance remains.
        for (auto plugin = loaded_plugins.rbegin(); plugin != loaded_plugins.rend(); ++plugin)
            plugin->instance.reset();

        loaded_plugins.clear();
    }

    void PluginUnloader::unload(
        LoadedPlugins& loaded_plugins,
        ServiceProvider& service_provider,
        PluginOwnershipTracker& ownership_tracker,
        IMessageCoordinator& coordinator)
    {
        unload_plugins(loaded_plugins, service_provider, ownership_tracker, std::ref(coordinator));
    }
}
