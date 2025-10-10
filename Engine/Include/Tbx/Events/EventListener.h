#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Memory/Refs.h"
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
        EventListener() = default;
        EventListener(WeakRef<EventBus> bus);
        ~EventListener();

        // TODO: If we want listeners to be moveable, we can't support instance based subscriptions...
        /*EventListener(const EventListener&) = delete;
        EventListener& operator=(const EventListener&) = delete;

        EventListener(EventListener&& other) noexcept
        {
            TransferFrom(std::move(other));
        }
        EventListener& operator=(EventListener&& other) noexcept
        {
            if (this != &other)
            {
                Unbind();
                TransferFrom(std::move(other));
            }
            return *this;
        }*/

        /// <summary>
        /// Binds the listener to the specified event bus via a weak reference.
        /// </summary>
        void Bind(WeakRef<EventBus> bus);

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

            const auto token = bus->Subscribe<TEvent>(std::move(callback));
            TrackToken(token);
            return token;
        }

        /// <summary>
        /// Subscribes an instance method to receive events of the given type.
        /// </summary>
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
        void TransferFrom(EventListener&& other) noexcept
        {
            Bind(other._bus);
            _activeTokens = std::move(other._activeTokens);
            other._activeTokens.clear();
        }
        void TrackToken(const Uid& token);
        Ref<EventBus> LockBus() const;

    private:
        WeakRef<EventBus> _bus = {};
        std::unordered_set<Uid> _activeTokens = {};
        mutable std::mutex _mutex = {};
    };
}

