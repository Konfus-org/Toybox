#pragma once
#include "tbx/events/queue.h"
#include <functional>
#include <vector>

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
    /// Purpose: Type-erased base so the event bus can hold signals of any event kind and purge an
    /// owner's subscriptions across all of them without naming their event types.
    struct TBX_DLL_EXPORT ISignal
    {
        virtual ~ISignal() = default;
        virtual void unsubscribe_owner(const void* owner) = 0;
    };

    /// @brief
    /// Purpose: A single event kind. Two flavours from one type: constructed with a Queue it is a
    /// deferred, thread-safe signal (emit enqueues, the pump drains) — how the global event bus and the
    /// EventsState signals work; constructed empty (the default) it is an inline signal that dispatches
    /// on emit() — how a component/toy signal member works, since systems raise those on the main thread.
    /// @details
    /// Ownership: Subscriptions are owner-tagged so script reloads / the editor bulk purge. Thread
    /// Safety: a queued signal's emit() is safe from any thread; subscribe/unsubscribe/inline-emit belong
    /// to the main thread, where dispatch also runs.
    template <typename TEvent>
    class Signal final : public ISignal
    {
      public:
        Signal() = default;
        explicit Signal(Queue& queue)
            : _queue(&queue)
        {
        }

        // A copy starts with no subscribers: a component signal copied during kit instantiation must not
        // inherit the source's listeners. Assignment likewise leaves our own subscribers untouched.
        Signal(const Signal&) noexcept {}
        Signal& operator=(const Signal&) noexcept
        {
            return *this;
        }
        Signal(Signal&&) = default;
        Signal& operator=(Signal&&) = default;

      public:
        /// @brief
        /// Purpose: Delivers an event — queued to the next pump drain when this signal has a queue,
        /// dispatched inline otherwise.
        void emit(const TEvent& event)
        {
            if (_queue != nullptr)
                _queue->push(this, &Signal::dispatch, event);
            else
                deliver(event);
        }

        /// @brief
        /// Purpose: Registers a handler under an owner tag (identity-only, never dereferenced) that
        /// groups subscriptions for bulk removal.
        Token subscribe(const void* owner, std::function<void(const TEvent&)> fn)
        {
            const Token token = _next_token++;
            _subscribers.push_back({.token = token, .owner = owner, .fn = std::move(fn)});
            return token;
        }

        /// @brief
        /// Purpose: Ergonomic subscribe with no owner tag (the signal owns it) — the C++ connect verb.
        Token connect(std::function<void(const TEvent&)> fn)
        {
            return subscribe(this, std::move(fn));
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
        void unsubscribe_owner(const void* owner) override
        {
            for (auto& subscriber : _subscribers)
                if (subscriber.owner == owner)
                    subscriber.fn = nullptr;
        }

      private:
        static void dispatch(void* signal, const void* payload)
        {
            static_cast<Signal*>(signal)->deliver(*static_cast<const TEvent*>(payload));
        }

        void deliver(const TEvent& event)
        {
            // Snapshot the count: handlers subscribed during dispatch see the NEXT event.
            const size count = _subscribers.size();
            for (size i = 0; i < count; ++i)
                if (_subscribers[i].fn)
                    _subscribers[i].fn(event);
            std::erase_if(
                _subscribers,
                [](const SignalSubscriber<TEvent>& subscriber)
                {
                    return subscriber.fn == nullptr;
                });
        }

      private:
        Queue* _queue = nullptr;
        std::vector<SignalSubscriber<TEvent>> _subscribers;
        Token _next_token = 1;
    };
}
