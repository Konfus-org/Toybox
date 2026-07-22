#pragma once
#include "tbx/events/event_queue.h"
#include <functional>

namespace tbx
{
    /// @brief
    /// Purpose: One subscriber slot on a Signal; a null fn is a tombstone swept after dispatch.
    template <typename TEvent>
    struct SignalSubscriber
    {
        Token token = 0;
        // Identity-only tag (never dereferenced) grouping subscriptions for bulk removal.
        const void* owner = nullptr;
        std::function<void(const TEvent&)> fn;
    };

    /// @brief
    /// Purpose: A single event kind, declared as a named member of the events state — adding an event
    /// means adding a member, deliberately.
    /// @details
    /// Ownership: Subscriptions are owner-tagged so script reloads / the future editor can bulk
    /// purge. Thread Safety: emit() is safe from any thread (it only enqueues); subscribe and
    /// unsubscribe belong to the main thread, where dispatch also runs.
    template <typename TEvent>
    class Signal final
    {
      public:
        explicit Signal(EventQueue& queue)
            : _queue(queue)
        {
        }

      public:
        /// @brief
        /// Purpose: Queues an event for delivery at the next pump drain.
        void emit(const TEvent& event)
        {
            _queue.get().push(this, &Signal::dispatch, event);
        }

        /// @brief
        /// Purpose: Registers a handler; the owner tag is identity-only (never dereferenced)
        /// and groups subscriptions for bulk removal.
        Token subscribe(const void* owner, std::function<void(const TEvent&)> fn)
        {
            const Token token = _next_token++;
            _subscribers.push_back({.token = token, .owner = owner, .fn = std::move(fn)});
            return token;
        }

        /// @brief
        /// Purpose: Removes one subscription by its token.
        void unsubscribe(Token token)
        {
            for (auto& subscriber : _subscribers)
                if (subscriber.token == token)
                    subscriber.fn = nullptr;
        }

        /// @brief
        /// Purpose: Removes every subscription registered under the given owner tag.
        void unsubscribe_owner(const void* owner)
        {
            for (auto& subscriber : _subscribers)
                if (subscriber.owner == owner)
                    subscriber.fn = nullptr;
        }

      private:
        static void dispatch(void* signal, const void* payload)
        {
            auto& self = *static_cast<Signal*>(signal);
            const auto& event = *static_cast<const TEvent*>(payload);
            // Snapshot the count: handlers subscribed during dispatch see the NEXT event.
            const size count = self._subscribers.size();
            for (size i = 0; i < count; ++i)
                if (self._subscribers[i].fn)
                    self._subscribers[i].fn(event);
            std::erase_if(
                self._subscribers,
                [](const SignalSubscriber<TEvent>& s)
                {
                    return s.fn == nullptr;
                });
        }

      private:
        std::reference_wrapper<EventQueue> _queue;
        std::vector<SignalSubscriber<TEvent>> _subscribers;
        Token _next_token = 1;
    };
}
