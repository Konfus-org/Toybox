#pragma once
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include <string>
#include <unordered_set>
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief One (address, key) event subscription row from the editor's sync.subscribe.
    struct EventSubscription
    {
        std::string address = {};
        std::string key = {};
        // Parsed from the address so raises match without re-parsing; zero when the address names
        // no entity.
        uint64 entity_id = 0U;
    };

    /// @brief
    /// Purpose: The engine side of the editor's sync.event channel: the table of (address, key)
    /// event subscriptions. Plain state: sync_event_ops owns the behavior (subscribe/unsubscribe and
    /// pushing raises). Cleared wholesale when the editor disconnects — its objects re-subscribe on
    /// bind.
    /// @details
    /// Ownership: Owned by the plugin by value. Thread Safety: Main-thread only.
    struct SyncEventState
    {
        std::vector<EventSubscription> subscriptions = {};

        // Entities whose trigger/collider components currently carry the bridge's forwarding
        // callbacks (physics_event_ops), so binding stays idempotent and detach can clear them. Bound
        // only while playing and dropped when the played world is rebuilt on leaving play.
        std::unordered_set<tbx::Uuid> bound_entities = {};
    };
}
