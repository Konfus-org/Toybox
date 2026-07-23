#include "tbx/events/events.h"

namespace tbx::internal
{
    void update_events(EventsState& state)
    {
        state.queue.drain();
    }

    void unsubscribe_all(EventsState& state, const void* owner)
    {
        for (auto& [type_hash, signal] : state.signals)
            signal->unsubscribe_owner(owner);
    }
}
