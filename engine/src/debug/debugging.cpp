#include "tbx/debug/debugging.h"
#include "runtime_state.h"
#include "view.h"

namespace tbx
{
    void internal::update_debugging(RuntimeState& state)
    {
        update_debug_view(
            state.debug,
            state.input,
            state.sandbox,
            state.assets,
            state.windows,
            state.ui,
            state.frame.delta_time);
    }
}