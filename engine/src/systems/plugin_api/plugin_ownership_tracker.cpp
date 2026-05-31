#include "tbx/systems/plugin_api/plugin_ownership_tracker.h"
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace tbx
{
    static std::weak_ptr<PluginOwnershipTracker> g_plugin_ownership_tracker = {};
    static std::mutex g_plugin_ownership_tracker_mutex = {};

    struct PluginOwnershipRecord
    {
        std::unordered_set<Uuid> entity_ids = {};
        std::unordered_set<Handle> pinned_asset_handles = {};
        std::unordered_set<std::filesystem::path> asset_directories = {};
        std::unordered_set<std::type_index> service_types = {};
        std::unordered_set<std::type_index> component_types = {};
        std::unordered_set<std::string> serializable_type_names = {};
        std::unordered_set<std::type_index> asset_types = {};
    };

    struct PluginOwnershipTracker::State
    {
        std::mutex mutex = {};
        std::unordered_map<Uuid, PluginOwnershipRecord> records_by_plugin_id = {};
    };

    static bool is_valid_plugin_instance_id(Uuid plugin_id)
    {
        return plugin_id.is_valid();
    }

    static std::type_index type_index_key(std::type_index value)
    {
        return value;
    }

    static std::string type_index_name(std::type_index value)
    {
        return std::string(value.name());
    }

    PluginOwnershipTracker::PluginOwnershipTracker()
        : _state(std::make_unique<State>())
    {
    }

    PluginOwnershipTracker::~PluginOwnershipTracker() noexcept = default;

    void PluginOwnershipTracker::track_entity(Uuid plugin_id, Uuid entity_id)
    {
        if (!is_valid_plugin_instance_id(plugin_id) || !entity_id.is_valid())
            return;

        auto guard = std::lock_guard(_state->mutex);
        _state->records_by_plugin_id[plugin_id].entity_ids.insert(entity_id);
    }

    void PluginOwnershipTracker::track_asset_pin(Uuid plugin_id, const Handle& handle)
    {
        if (!is_valid_plugin_instance_id(plugin_id) || !handle.is_valid())
            return;

        auto guard = std::lock_guard(_state->mutex);
        _state->records_by_plugin_id[plugin_id].pinned_asset_handles.insert(handle);
    }

    void PluginOwnershipTracker::track_asset_directory(
        Uuid plugin_id,
        const std::filesystem::path& path)
    {
        if (!is_valid_plugin_instance_id(plugin_id) || path.empty())
            return;

        auto guard = std::lock_guard(_state->mutex);
        _state->records_by_plugin_id[plugin_id].asset_directories.insert(path.lexically_normal());
    }

    void PluginOwnershipTracker::track_service(Uuid plugin_id, std::type_index service_type)
    {
        if (!is_valid_plugin_instance_id(plugin_id))
            return;

        auto guard = std::lock_guard(_state->mutex);
        _state->records_by_plugin_id[plugin_id].service_types.insert(type_index_key(service_type));
    }

    void PluginOwnershipTracker::track_component_registration(
        Uuid plugin_id,
        std::type_index component_type)
    {
        if (!is_valid_plugin_instance_id(plugin_id))
            return;

        auto guard = std::lock_guard(_state->mutex);
        _state->records_by_plugin_id[plugin_id].component_types.insert(
            type_index_key(component_type));
    }

    void PluginOwnershipTracker::track_serializable_registration(
        Uuid plugin_id,
        std::string registration_name)
    {
        if (!is_valid_plugin_instance_id(plugin_id) || registration_name.empty())
            return;

        auto guard = std::lock_guard(_state->mutex);
        _state->records_by_plugin_id[plugin_id].serializable_type_names.insert(
            std::move(registration_name));
    }

    void PluginOwnershipTracker::track_asset_type_registration(
        Uuid plugin_id,
        std::type_index asset_type)
    {
        if (!is_valid_plugin_instance_id(plugin_id))
            return;

        auto guard = std::lock_guard(_state->mutex);
        _state->records_by_plugin_id[plugin_id].asset_types.insert(type_index_key(asset_type));
    }

    OwnedPluginResources PluginOwnershipTracker::snapshot_and_clear(Uuid plugin_id)
    {
        auto resources = OwnedPluginResources {};
        if (!is_valid_plugin_instance_id(plugin_id))
            return resources;

        auto guard = std::lock_guard(_state->mutex);
        const auto iterator = _state->records_by_plugin_id.find(plugin_id);
        if (iterator == _state->records_by_plugin_id.end())
            return resources;

        const auto& record = iterator->second;
        resources.entity_ids.assign(record.entity_ids.begin(), record.entity_ids.end());
        resources.pinned_asset_handles.assign(
            record.pinned_asset_handles.begin(),
            record.pinned_asset_handles.end());
        resources.asset_directories.assign(
            record.asset_directories.begin(),
            record.asset_directories.end());
        resources.service_types.assign(record.service_types.begin(), record.service_types.end());
        resources.component_types.assign(
            record.component_types.begin(),
            record.component_types.end());
        resources.serializable_type_names.assign(
            record.serializable_type_names.begin(),
            record.serializable_type_names.end());
        resources.asset_types.assign(record.asset_types.begin(), record.asset_types.end());
        _state->records_by_plugin_id.erase(iterator);

        std::sort(resources.entity_ids.begin(), resources.entity_ids.end());
        std::sort(
            resources.pinned_asset_handles.begin(),
            resources.pinned_asset_handles.end(),
            [](const Handle& left, const Handle& right)
            {
                if (left.id != right.id)
                    return left.id < right.id;
                return left.name < right.name;
            });
        std::sort(
            resources.asset_directories.begin(),
            resources.asset_directories.end(),
            [](const std::filesystem::path& left, const std::filesystem::path& right)
            {
                return left.generic_string() < right.generic_string();
            });
        std::sort(
            resources.service_types.begin(),
            resources.service_types.end(),
            [](std::type_index left, std::type_index right)
            {
                return type_index_name(left) < type_index_name(right);
            });
        std::sort(
            resources.component_types.begin(),
            resources.component_types.end(),
            [](std::type_index left, std::type_index right)
            {
                return type_index_name(left) < type_index_name(right);
            });
        std::sort(
            resources.serializable_type_names.begin(),
            resources.serializable_type_names.end());
        std::sort(
            resources.asset_types.begin(),
            resources.asset_types.end(),
            [](std::type_index left, std::type_index right)
            {
                return type_index_name(left) < type_index_name(right);
            });
        return resources;
    }

    void PluginOwnershipTracker::clear()
    {
        auto guard = std::lock_guard(_state->mutex);
        _state->records_by_plugin_id.clear();
    }

    void bind_plugin_ownership_tracker(std::weak_ptr<PluginOwnershipTracker> tracker)
    {
        auto guard = std::lock_guard(g_plugin_ownership_tracker_mutex);
        g_plugin_ownership_tracker = std::move(tracker);
    }

    std::shared_ptr<PluginOwnershipTracker> lock_plugin_ownership_tracker()
    {
        auto guard = std::lock_guard(g_plugin_ownership_tracker_mutex);
        return g_plugin_ownership_tracker.lock();
    }
}
