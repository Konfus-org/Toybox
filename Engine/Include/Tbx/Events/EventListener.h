#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Memory/Refs.h"
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

    /// <summary>
    /// Represents a member function callback that handles an event instance.
    /// </summary>
    /// <typeparam name="T">The subscriber type that owns the member function.</typeparam>
    /// <typeparam name="TEvent">The event type handled by the callback.</typeparam>
    template <typename T, class TEvent>
    using ClassEventHandlerFunction = void(T::*)(TEvent&);

    class TBX_EXPORT EventListener
    {
    public:
        /// <summary>
        /// Creates an unbound listener.
        /// </summary>
        EventListener() = default;

        /// <summary>
        /// Creates a listener bound to the supplied event bus reference.
        /// </summary>
        /// <param name="bus">A weak reference to the event bus that will service subscriptions.</param>
        EventListener(WeakRef<EventBus> bus);

        /// <summary>
        /// Unsubscribes the listener from all events and releases the bus reference.
        /// </summary>
        ~EventListener();

        /// <summary>
        /// Binds the listener to the specified event bus via a weak reference.
        /// </summary>
        /// <param name="bus">The weak reference to the event bus providing subscription services.</param>
        void Bind(WeakRef<EventBus> bus);

        /// <summary>
        /// Cancels all active subscriptions held by this listener.
        /// </summary>
        void Unbind();

        /// <summary>
        /// Determines whether the listener is bound to an event bus.
        /// </summary>
        /// <returns><c>true</c> when a bus is bound; otherwise, <c>false</c>.</returns>
        bool IsBound() const;

        /// <summary>
        /// Subscribes a callable to receive events of the given type.
        /// </summary>
        /// <typeparam name="TEvent">The event type to listen for.</typeparam>
        /// <typeparam name="TCallable">The callable type to invoke for each event.</typeparam>
        /// <param name="callable">The callable invoked with the event reference.</param>
        template <class TEvent, typename TCallable>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>> &&
                 std::is_invocable_r_v<void, TCallable, TEvent&>
        Uid Listen(TCallable&& callable)
        {
            auto bus = LockBus();
            TBX_ASSERT(bus, "EventListener: Cannot listen without a bound event bus.");

            EventCallback callback = [fn = std::forward<TCallable>(callable)](Event& event)
            {
                std::invoke(fn, static_cast<TEvent&>(event));
            };

            const auto token = bus->Subscribe<TEvent>(std::move(callback));
            TrackToken(token);
            return token;
        }

        /// <summary>
        /// Subscribes an instance method to receive events of the given type.
        /// </summary>
        /// <typeparam name="TSubscriber">The type of the subscribing instance.</typeparam>
        /// <typeparam name="TEvent">The event type to listen for.</typeparam>
        /// <param name="instance">The object instance whose member function should handle events.</param>
        /// <param name="callback">The member function invoked for each event.</param>
        template <typename TSubscriber, class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        Uid Listen(TSubscriber* instance, ClassEventHandlerFunction<TSubscriber, TEvent> callback)
        {
            TBX_ASSERT(instance != nullptr, "EventListener: Cannot subscribe with a null instance.");
            const auto token = Listen<TEvent>([instance, callback](TEvent& event)
            {
                std::invoke(callback, instance, event);
            });
            return token;

        }

        /// <summary>
        /// Subscribes a free-function callback to receive events of the given type.
        /// </summary>
        /// <typeparam name="TEvent">The event type to listen for.</typeparam>
        /// <param name="callback">The function pointer invoked for each event.</param>
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
        /// <summary>
        /// Tracks an issued subscription token for later cleanup.
        /// </summary>
        /// <param name="token">The token returned during subscription.</param>
        void TrackToken(const Uid& token);

        /// <summary>
        /// Locks the weak event bus reference into a strong reference.
        /// </summary>
        /// <returns>A strong reference to the event bus, or an empty reference if the bus expired.</returns>
        Ref<EventBus> LockBus() const;

    private:
        WeakRef<EventBus> _bus = {};
        std::unordered_set<Uid> _activeTokens = {};
    };
}

