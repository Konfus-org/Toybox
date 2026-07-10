#include "plugin_ownership_tracker.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracking.h"
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace tbx
{
    struct PluginOwnershipRecord
    {
        std::unordered_set<Uuid> entity_ids = {};
        std::unordered_set<Handle> pinned_asset_handles = {};
        std::unordered_set<std::filesystem::path> asset_directories = {};
        std::vector<std::type_index> service_types = {};
        std::unordered_set<std::type_index> component_types = {};
        std::unordered_set<std::string> serializable_type_names = {};
        std::unordered_set<std::type_index> asset_types = {};
        std::unordered_set<std::string> script_type_names = {};
    };

    struct PluginOwnershipTracker::State
    {
        std::mutex mutex = {};
        std::unordered_map<Uuid, PluginOwnershipRecord> records_by_plugin_id = {};
    };

    class PluginOwnershipTrackerBinding final
    {
      public:
        static PluginOwnershipTrackerBinding& get_instance()
        {
            static PluginOwnershipTrackerBinding binding = {};
            return binding;
        }

      public:
        PluginOwnershipTrackerBinding(const PluginOwnershipTrackerBinding&) = delete;
        PluginOwnershipTrackerBinding& operator=(const PluginOwnershipTrackerBinding&) = delete;
        PluginOwnershipTrackerBinding(PluginOwnershipTrackerBinding&&) = delete;
        PluginOwnershipTrackerBinding& operator=(PluginOwnershipTrackerBinding&&) = delete;

      public:
        void bind(std::weak_ptr<PluginOwnershipTracker> tracker)
        {
            auto guard = std::lock_guard(_mutex);
            _tracker = std::move(tracker);
        }

        std::shared_ptr<PluginOwnershipTracker> lock()
        {
            auto guard = std::lock_guard(_mutex);
            return _tracker.lock();
        }

      private:
        PluginOwnershipTrackerBinding() = default;
        ~PluginOwnershipTrackerBinding() noexcept = default;

      private:
        std::mutex _mutex = {};
        std::weak_ptr<PluginOwnershipTracker> _tracker = {};
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
        auto& service_types = _state->records_by_plugin_id[plugin_id].service_types;
        const auto key = type_index_key(service_type);
        if (std::ranges::find(service_types, key) == service_types.end())
            service_types.push_back(key);
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

    void PluginOwnershipTracker::track_script_registration(
        Uuid plugin_id,
        std::string registration_name)
    {
        if (!is_valid_plugin_instance_id(plugin_id) || registration_name.empty())
            return;

        auto guard = std::lock_guard(_state->mutex);
        _state->records_by_plugin_id[plugin_id].script_type_names.insert(std::move(registration_name));
    }

    void PluginOwnershipTracker::track_asset_type(
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
        resources.script_type_names.assign(
            record.script_type_names.begin(),
            record.script_type_names.end());
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
        std::reverse(resources.service_types.begin(), resources.service_types.end());
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
        std::sort(resources.script_type_names.begin(), resources.script_type_names.end());
        return resources;
    }

    void PluginOwnershipTracker::clear()
    {
        auto guard = std::lock_guard(_state->mutex);
        _state->records_by_plugin_id.clear();
    }

    static std::shared_ptr<PluginOwnershipTracker> lock_bound_tracker()
    {
        return PluginOwnershipTrackerBinding::get_instance().lock();
    }

    static Uuid get_tracked_plugin_id()
    {
        if (!has_active_plugin_id())
            return {};

        return get_active_plugin_id();
    }

    void bind_plugin_ownership_tracker(std::weak_ptr<PluginOwnershipTracker> tracker)
    {
        PluginOwnershipTrackerBinding::get_instance().bind(std::move(tracker));
    }

    std::shared_ptr<PluginOwnershipTracker> lock_plugin_ownership_tracker()
    {
        return lock_bound_tracker();
    }

    void track_plugin_owned_asset_directory(const std::filesystem::path& path)
    {
        const auto plugin_id = get_tracked_plugin_id();
        if (!plugin_id.is_valid())
            return;

        if (const auto tracker = lock_bound_tracker())
            tracker->track_asset_directory(plugin_id, path);
    }

    void track_plugin_owned_asset_pin(const Handle& handle)
    {
        const auto plugin_id = get_tracked_plugin_id();
        if (!plugin_id.is_valid())
            return;

        if (const auto tracker = lock_bound_tracker())
            tracker->track_asset_pin(plugin_id, handle);
    }

    void track_plugin_owned_asset_type(std::type_index asset_type)
    {
        const auto plugin_id = get_tracked_plugin_id();
        if (!plugin_id.is_valid())
            return;

        if (const auto tracker = lock_bound_tracker())
            tracker->track_asset_type(plugin_id, asset_type);
    }

    void track_plugin_owned_component_registration(std::type_index component_type)
    {
        const auto plugin_id = get_tracked_plugin_id();
        if (!plugin_id.is_valid())
            return;

        if (const auto tracker = lock_bound_tracker())
            tracker->track_component_registration(plugin_id, component_type);
    }

    void track_plugin_owned_entity(Uuid entity_id)
    {
        const auto plugin_id = get_tracked_plugin_id();
        if (!plugin_id.is_valid())
            return;

        if (const auto tracker = lock_bound_tracker())
            tracker->track_entity(plugin_id, entity_id);
    }

    void track_plugin_owned_serializable_registration(std::string_view registration_name)
    {
        const auto plugin_id = get_tracked_plugin_id();
        if (!plugin_id.is_valid() || registration_name.empty())
            return;

        if (const auto tracker = lock_bound_tracker())
            tracker->track_serializable_registration(plugin_id, std::string(registration_name));
    }

    void track_plugin_owned_script_registration(std::string_view registration_name)
    {
        const auto plugin_id = get_tracked_plugin_id();
        if (!plugin_id.is_valid() || registration_name.empty())
            return;

        if (const auto tracker = lock_bound_tracker())
            tracker->track_script_registration(plugin_id, std::string(registration_name));
    }

    void track_plugin_owned_service_registration(std::type_index service_type)
    {
        const auto plugin_id = get_tracked_plugin_id();
        if (!plugin_id.is_valid())
            return;

        if (const auto tracker = lock_bound_tracker())
            tracker->track_service(plugin_id, service_type);
    }
}
