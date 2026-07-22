#include "tbx/events/events.h"

namespace tbx::events
{
    void update(State& state)
    {
        state.queue.drain();
    }
}
