#include "tbx/events/events.h"

namespace tbx
{
    void update_events(EventsState& state)
    {
        state.queue.drain();
    }

    void unsubscribe_all(EventsState& state, const void* owner)
    {
        state.key.unsubscribe_owner(owner);
        state.window_resized.unsubscribe_owner(owner);
        state.asset_reloaded.unsubscribe_owner(owner);
        state.asset_unloaded.unsubscribe_owner(owner);
        state.script_reloaded.unsubscribe_owner(owner);
        state.collision.unsubscribe_owner(owner);
        state.input_device_connected.unsubscribe_owner(owner);
        state.input_device_disconnected.unsubscribe_owner(owner);
    }
}
