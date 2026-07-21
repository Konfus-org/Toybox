#include "tbx/events/events.h"
#include <memory>

namespace tbx::events
{
    /// @brief
    /// Purpose: The module's whole state: the queue and every named signal over it.
    struct EventsState
    {
        EventQueue queue;
        Signal<KeyEvent> key {queue};
        Signal<WindowResized> window_resized {queue};
        Signal<AssetReloaded> asset_reloaded {queue};
        Signal<AssetUnloaded> asset_unloaded {queue};
        Signal<ScriptReloaded> script_reloaded {queue};
        Signal<CollisionEvent> collision {queue};
    };

    static std::unique_ptr<EventsState> g_events = {};

    static EventsState& ensure_events_ready()
    {
        if (!g_events)
            g_events = std::make_unique<EventsState>();
        return *g_events;
    }

    Signal<AssetReloaded>& asset_reloaded()
    {
        return ensure_events_ready().asset_reloaded;
    }

    Signal<AssetUnloaded>& asset_unloaded()
    {
        return ensure_events_ready().asset_unloaded;
    }

    Signal<CollisionEvent>& collision()
    {
        return ensure_events_ready().collision;
    }

    Signal<KeyEvent>& key()
    {
        return ensure_events_ready().key;
    }

    Signal<ScriptReloaded>& script_reloaded()
    {
        return ensure_events_ready().script_reloaded;
    }

    Signal<WindowResized>& window_resized()
    {
        return ensure_events_ready().window_resized;
    }

    void drain()
    {
        if (g_events)
            g_events->queue.drain();
    }

    void purge()
    {
        g_events.reset();
    }
}
