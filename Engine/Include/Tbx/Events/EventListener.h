#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Memory/Hashing.h"
#include <mutex>
#include <type_traits>
#include <unordered_set>

namespace Tbx
{
    /// <summary>
    /// Represents a free-function callback that handles an event instance.
    /// </summary>
    /// <typeparam name="TEvent">The event type handled by the callback.</typeparam>
    template <class TEvent>
    using EventHandlerFunction = void(*)(TEvent&);

    class TBX_EXPORT EventListener
    {
    public:
        EventListener() = default;
        EventListener(Ref<EventBus> bus);
        ~EventListener();

        /// <summary>
        /// Binds the listener to the specified event bus.
        /// </summary>
        void Bind(Ref<EventBus> bus);

        /// <summary>
        /// Cancels all active subscriptions held by this listener.
        /// </summary>
        void Unbind();

        /// <summary>
        /// Determines whether the listener is bound to an event bus.
        /// </summary>
        bool IsBound() const;

        /// <summary>
        /// Subscribes a callable to receive events of the given type.
        /// </summary>
        template <class TEvent, typename TCallable>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>> && std::is_invocable_r_v<void, TCallable, TEvent&>
        Uid Listen(TCallable&& callable)
        {
            auto bus = LockBus();
            TBX_ASSERT(bus, "EventListener: Cannot listen without a bound event bus.");

            EventCallback callback = [fn = std::forward<TCallable>(callable)](Event& event)
            {
                std::invoke(fn, static_cast<TEvent&>(event));
            };

            const auto eventKey = Hash<std::decay_t<TEvent>>();
            const auto token = bus->AddSubscription(eventKey, std::move(callback));
            TrackToken(token);
            return token;
        }

        /// <summary>
        /// Subscribes a free-function callback to receive events of the given type.
        /// </summary>
        template <class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        Uid Listen(EventHandlerFunction<TEvent> callback)
        {
            TBX_ASSERT(callback != nullptr, "EventListener: Cannot subscribe a null callback.");
            const auto token = Listen<TEvent>([callback](TEvent& event)
            {
                callback(event);
            });
            return token;
        }

        /// <summary>
        /// Unsubscribes the listener from the specified event type.
        /// </summary>
        /// <param name="token"></param>
        void StopListening(const Uid& token);

    private:
        static void Transfer(EventListener& from, EventListener& to) noexcept;
        void TrackToken(const Uid& token);
        Ref<EventBus> LockBus() const;
        void ReleaseSubscriptions();

    private:
        Ref<EventBus> _bus = EventBus::Global;
        std::unordered_set<Uid> _activeTokens = {};
        mutable std::mutex _mutex = {};
    };
}

