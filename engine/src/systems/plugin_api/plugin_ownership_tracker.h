#pragma once
#include <memory>
#include <string>
#include <vector>

namespace tbx
{
    struct OwnedPluginResources
    {
        std::vector<Uuid> entity_ids = {};
        std::vector<Handle> pinned_asset_handles = {};
        std::vector<std::filesystem::path> asset_directories = {};
        std::vector<std::type_index> service_types = {};
        std::vector<std::type_index> component_types = {};
        std::vector<std::string> serializable_type_names = {};
        std::vector<std::type_index> asset_types = {};
        std::vector<std::string> script_type_names = {};
    };

    class PluginOwnershipTracker final
    {
      public:
        PluginOwnershipTracker();
        ~PluginOwnershipTracker() noexcept;

      public:
        PluginOwnershipTracker(const PluginOwnershipTracker&) = delete;
        PluginOwnershipTracker& operator=(const PluginOwnershipTracker&) = delete;
        PluginOwnershipTracker(PluginOwnershipTracker&&) = delete;
        PluginOwnershipTracker& operator=(PluginOwnershipTracker&&) = delete;

      public:
        void track_asset_directory(Uuid plugin_id, const std::filesystem::path& path);
        void track_asset_pin(Uuid plugin_id, const Handle& handle);
        void track_asset_type(Uuid plugin_id, std::type_index asset_type);
        void track_component_registration(Uuid plugin_id, std::type_index component_type);
        void track_entity(Uuid plugin_id, Uuid entity_id);
        void track_serializable_registration(Uuid plugin_id, std::string registration_name);
        void track_script_registration(Uuid plugin_id, std::string registration_name);
        void track_service(Uuid plugin_id, std::type_index service_type);

        OwnedPluginResources snapshot_and_clear(Uuid plugin_id);
        void clear();

      private:
        struct State;

      private:
        std::unique_ptr<State> _state = {};
    };

    void bind_plugin_ownership_tracker(std::weak_ptr<PluginOwnershipTracker> tracker);
    std::shared_ptr<PluginOwnershipTracker> lock_plugin_ownership_tracker();
}
