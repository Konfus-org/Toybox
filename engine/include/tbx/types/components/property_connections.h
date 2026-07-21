#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/components/component.h"
#include "tbx/types/components/property_connections.generated.h"
#include "tbx/types/uuid.h"
#include <string>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: One live property-to-property link: each tick the source entity's component property
    /// drives the owning entity's target property (the editor's value wires).
    /// @details
    /// Owned by the TARGET's entity (the driven side), so a connection streams, saves and duplicates
    /// with the entity it drives, and a broken source shows as a broken wire rather than lost data.
    /// Fields are the sync-path property identities: component wire names + property wire names.
    [[serializable]];
    struct TBX_API PropertyConnection
    {
        Uuid source_entity = {};

        std::string source_component = {};

        std::string source_field = {};

        std::string target_component = {};

        std::string target_field = {};
    };

    /// @brief
    /// Purpose: Stores the property connections driving an entity's components — evaluated once per
    /// tick, before scripts run, by the PropertyConnectionSystem.
    [[serializable]];
    struct TBX_API PropertyConnections : Component
    {
        std::vector<PropertyConnection> connections = {};
    };
}
