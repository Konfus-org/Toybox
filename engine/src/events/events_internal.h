#pragma once
#include "tbx/events/events.h"
#include <functional>

namespace tbx::internal
{
    /// @brief
    /// Purpose: Subscribes a handler to app-wide event type E on the given bus; owner tags it for
    /// bulk purge. Returns a Token. The testable form (explicit bus) — the public tbx::on_event
    /// (events_api.h) inlines the same over get_runtime().events.
    template <typename TEvent>
    Token on_event(EventsState& events, const void* owner, std::function<void(const TEvent&)> handler)
    {
        return events.signal<TEvent>().subscribe(owner, std::move(handler));
    }

    /// @brief
    /// Purpose: Raises an app-wide event on the given bus (queued to the next pump drain).
    template <typename TEvent>
    void raise_event(EventsState& events, const TEvent& event)
    {
        events.signal<TEvent>().emit(event);
    }

    /// @brief
    /// Purpose: Drops every subscription registered under the given owner tag from every signal at
    /// once — the bulk teardown a language backend runs before it tears down. Generic over the bus.
    void unsubscribe_all(EventsState& state, const void* owner);

    /// @brief
    /// Purpose: Dispatches everything queued since the last update, in emission order — the events
    /// module's per-frame verb; tbx::run() calls it during the pump. Events emitted during an update
    /// land in the next one.
    void update_events(EventsState& state);
}
