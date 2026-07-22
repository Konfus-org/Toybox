#include "tbx/events/events.h"

namespace tbx::events
{
    void update(EventsState& state)
    {
        state.queue.drain();
    }
}
