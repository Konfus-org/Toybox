#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct ViewState;

    // The property-connection edits (the editor's value wires): one source→target component-property
    // link, stored on the TARGET's entity (its PropertyConnections component) so a connection streams,
    // saves and duplicates with the entity it drives. One driver per target property — adding over an
    // existing link replaces it. Types are checked at connect time (the serialized nodes' type tokens
    // must match, with numbers allowed to cross int/float). Main-thread only (request handling).

    /// @brief Adds (or replaces) the connection driving { entityId, component, property } from
    /// { sourceEntityId, sourceComponent, sourceProperty }.
    Result add_connection(
        const EngineServices& services, ViewState& views, const tbx::Json& params);

    /// @brief Removes the connection driving { entityId, component, property }.
    Result remove_connection(
        const EngineServices& services, ViewState& views, const tbx::Json& params);
}
