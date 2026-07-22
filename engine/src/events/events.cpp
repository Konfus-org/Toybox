#include "tbx/events/events.h"

namespace tbx
{
    void update_events(EventsState& state)
    {
        state.queue.drain();
    }
}
