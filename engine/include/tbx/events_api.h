#pragma once
#include "tbx/events/events.h"
#include "tbx/runtime.h"
#include <functional>

// The public app-wide event API: on_event<E>/raise_event<E> over the running runtime. Thin wrappers
// over the running runtime's event bus (internal::get_runtime().events) — subscribe/emit on the
// type-keyed Signal<E>. Main-thread only. Scripts reach the same bus through the generated
// tbx.events.<E>:connect / :raise bindings.
namespace tbx
{
    /// @brief
    /// Purpose: Subscribes a handler to app-wide event type E on the running runtime; returns a Token.
    template <typename TEvent>
    Token on_event(std::function<void(const TEvent&)> handler)
    {
        return internal::get_runtime().events.signal<TEvent>().subscribe(nullptr, std::move(handler));
    }

    /// @brief
    /// Purpose: Raises an app-wide event on the running runtime (delivered at the next pump drain).
    template <typename TEvent>
    void raise_event(const TEvent& event)
    {
        internal::get_runtime().events.signal<TEvent>().emit(event);
    }
}
