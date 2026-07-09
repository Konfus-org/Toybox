#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <string_view>

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct SyncEventState;

    /// @brief Adds one { address, key } subscription (the editor's sync.subscribe). A duplicate
    /// subscribe collapses onto the existing row.
    Result subscribe_sync_event(SyncEventState& events, const tbx::Json& params);

    /// @brief Removes one { address, key } subscription (the editor's sync.unsubscribe).
    Result unsubscribe_sync_event(SyncEventState& events, const tbx::Json& params);

    /// @brief Sends `args` as one sync.event raise to every subscription matching the entity and
    /// key, addressed by the subscription's own address string (echoed back verbatim, so the
    /// editor's routing matches whatever template it subscribed with).
    void raise_sync_event_for_entity(
        const SyncEventState& events,
        const EngineServices& services,
        uint64 entity_id,
        std::string_view key,
        const tbx::Json& args);
}
