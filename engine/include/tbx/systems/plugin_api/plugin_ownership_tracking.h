#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include "tbx/types/uuid.h"
#include <filesystem>
#include <string_view>
#include <typeindex>

namespace tbx
{
    // TODO: Clean this up, lets make a generic resource tracker that is done via
    // ResourceTracker::get_instance().track(...); Then anything that needs to track resources
    // calls that track method. Then expose a ResourceTrackingScope(this) that we use to bind the
    // resource tracker via its ResourceTracker::get_instance().bind_owner(ResourceOwner...); then
    // we need an ResourceOwner that is friends with the ResourceTracker. Owners just need to
    // inherit from the ResourceOwner and use the scope, but the details are hidden in private
    // methods the inherit and scope are the only parts that should be used.
    /// @brief
    /// Purpose: Records a plugin-owned asset directory for later cleanup during unload.
    /// @details
    /// Ownership: Does not take ownership of the path; copies the normalized value when tracking
    /// is active.
    /// Thread Safety: Delegates synchronization to the active plugin manager tracker.
    TBX_API void track_plugin_owned_asset_directory(const std::filesystem::path& path);

    /// @brief
    /// Purpose: Records a plugin-owned pinned asset handle for later cleanup during unload.
    /// @details
    /// Ownership: Does not take ownership of the handle; copies it when tracking is active.
    /// Thread Safety: Delegates synchronization to the active plugin manager tracker.
    TBX_API void track_plugin_owned_asset_pin(const Handle& handle);

    /// @brief
    /// Purpose: Records a plugin-owned asset-type registration for later cleanup during unload.
    /// @details
    /// Ownership: Does not take ownership of the type index.
    /// Thread Safety: Delegates synchronization to the active plugin manager tracker.
    TBX_API void track_plugin_owned_asset_type(std::type_index asset_type);

    /// @brief
    /// Purpose: Records a plugin-owned component registration for later cleanup during unload.
    /// @details
    /// Ownership: Does not take ownership of the type index.
    /// Thread Safety: Delegates synchronization to the active plugin manager tracker.
    TBX_API void track_plugin_owned_component_registration(std::type_index component_type);

    /// @brief
    /// Purpose: Records a plugin-owned entity for later cleanup during unload.
    /// @details
    /// Ownership: Does not take ownership of the entity id.
    /// Thread Safety: Delegates synchronization to the active plugin manager tracker.
    TBX_API void track_plugin_owned_entity(Uuid entity_id);

    /// @brief
    /// Purpose: Records a plugin-owned serializable registration for later cleanup during unload.
    /// @details
    /// Ownership: Copies the registration name when tracking is active.
    /// Thread Safety: Delegates synchronization to the active plugin manager tracker.
    TBX_API void track_plugin_owned_serializable_registration(std::string_view registration_name);

    /// @brief
    /// Purpose: Records a plugin-owned service registration for later cleanup during unload.
    /// @details
    /// Ownership: Does not take ownership of the type index.
    /// Thread Safety: Delegates synchronization to the active plugin manager tracker.
    TBX_API void track_plugin_owned_service_registration(std::type_index service_type);
}
