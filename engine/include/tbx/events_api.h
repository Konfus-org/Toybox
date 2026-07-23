#pragma once
#include "tbx/events/events.h"
#include "tbx/runtime.h"
#include <functional>

// The public app-wide event API: on_event<E>/raise_event<E> over the running runtime. Thin wrappers —
// they resolve the bus through tbx::internal::get_runtime() and forward to the testable tbx::internal versions
// (events.h), which take the EventsState explicitly. Main-thread only. Scripts reach the same bus through
// the generated tbx.events.<E>:connect / :raise bindings.
namespace tbx
{
    /// @brief
    /// Purpose: Subscribes a handler to app-wide event type E on the running runtime; returns a Token.
    template <typename TEvent>
    Token on_event(std::function<void(const TEvent&)> handler)
    {
        return internal::on_event<TEvent>(internal::get_runtime().events, nullptr, std::move(handler));
    }

    /// @brief
    /// Purpose: Raises an app-wide event on the running runtime (delivered at the next pump drain).
    template <typename TEvent>
    void raise_event(const TEvent& event)
    {
        internal::raise_event<TEvent>(internal::get_runtime().events, event);
    }
}
