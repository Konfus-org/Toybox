#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Callbacks/CallbackFunction.h"
#include "Tbx/TypeAliases/Int.h"
#include <unordered_map>
#include <typeindex>
#include <vector>
#include <mutex>
#include <atomic>
#include "Tbx/TypeAliases/Pointers.h"

namespace Tbx
{
    template <class TEvent>
    using EventHandlerFunction = void(*)(TEvent&);

    template <class TEvent>
    using ConstEventHandlerFunction = void(*)(const TEvent&);

    template <typename T, class TEvent>
    using ClassEventHandlerFunction = void(T::*)(TEvent&);

    template <class TEvent, typename T>
    using ClassConstEventHandlerFunction = void(T::*)(const TEvent&);

    /// <summary>
    /// A callback function that takes an event as a parameter.
    /// </summary>
    using EventCallback = CallbackFunction<Event>;

    /// <summary>
    /// ALL events on ALL event busses will be suppressed during the lifetime of this class.
    /// Use with care.
    /// </summary>
    class EventSuppressor
    {
    public:
        EXPORT EventSuppressor();
        EXPORT ~EventSuppressor();

        EXPORT static bool IsSuppressing();

    private:
        static void Suppress();
        static void Unsuppress();

        static std::atomic_int _suppressCount;
    };

    /// <summary>
    /// A class that manages event subscriptions and sends events to subscribers.
    /// </summary>
    class EventBus
    {
    public:
        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// </summary>
        template <class TEvent>
        EXPORT void Subscribe(EventHandlerFunction<TEvent> callback)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            auto eventKey = GetEventHash<TEvent>();
            auto callbackKey = GetCallbackHash(callback);

            std::lock_guard<std::mutex> lock(_mutex);
            if (_subscribers.contains(eventKey) == false)
            {
                _subscribers[eventKey] = {};
            }

            _subscribers[eventKey][callbackKey] = [callback](Event& event)
            {
                callback(static_cast<TEvent&>(event));
            };
        }

        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// </summary>
        template <class TEvent>
        EXPORT void Subscribe(ConstEventHandlerFunction<TEvent> callback)
        {
            Subscribe(reinterpret_cast<EventHandlerFunction<TEvent>>(callback));
        }

        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// </summary>
        template <typename TSubscriber, class TEvent>
        EXPORT void Subscribe(TSubscriber* instance, ClassEventHandlerFunction<TSubscriber, TEvent> callback)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            auto eventKey = GetEventHash<TEvent>();
            auto callbackKey = GetCallbackHash(instance, callback);

            std::lock_guard<std::mutex> lock(_mutex);
            if (_subscribers.contains(eventKey) == false)
            {
                _subscribers[eventKey] = {};
            }

            _subscribers[eventKey][callbackKey] = [instance, callback](Event& event)
            {
                (instance->*callback)(static_cast<TEvent&>(event));
            };
        }

        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// </summary>
        template <typename TSubscriber, class TEvent>
        EXPORT void Subscribe(TSubscriber* instance, ClassConstEventHandlerFunction<TSubscriber, TEvent> callback)
        {
            Subscribe(instance, reinterpret_cast<ClassEventHandlerFunction<TSubscriber, TEvent>>(callback));
        }

        /// <summary>
        /// Removes the method associated with the given function from the list of callbacks for an event.
        /// </summary>
        template <class TEvent>
        EXPORT void Unsubscribe(EventHandlerFunction<TEvent> callback)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            auto eventKey = GetEventHash<TEvent>();

            std::lock_guard<std::mutex> lock(_mutex);
            if (_subscribers.contains(eventKey) == false)
            {
                return;
            }

            auto callbackKey = GetCallbackHash(callback);
            auto& callbacks = _subscribers[eventKey];
            if (callbacks.erase(callbackKey) == 0)
            {
                const auto& eventInfo = typeid(TEvent);
                TBX_ASSERT(false, "Failed to unsubscribe from event. Callback not found!", eventInfo.name());
            }

            if (callbacks.empty())
            {
                _subscribers.erase(eventKey);
            }
        }

        template <class TEvent>
        EXPORT void Unsubscribe(ConstEventHandlerFunction<TEvent> callback)
        {
            Unsubscribe(reinterpret_cast<EventHandlerFunction<TEvent>>(callback));
        }

        template <typename TSubscriber, class TEvent>
        EXPORT void Unsubscribe(TSubscriber* instance, ClassEventHandlerFunction<TSubscriber, TEvent> callback)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            auto eventKey = GetEventHash<TEvent>();

            std::lock_guard<std::mutex> lock(_mutex);
            if (_subscribers.contains(eventKey) == false)
            {
                return;
            }

            auto callbackKey = GetCallbackHash(instance, callback);
            auto& callbacks = _subscribers[eventKey];
            if (callbacks.erase(callbackKey) == 0)
            {
                const auto& eventInfo = typeid(TEvent);
                TBX_ASSERT(false, "Failed to unsubscribe from event {}. Callback not found!", eventInfo.name());
            }

            if (callbacks.empty())
            {
                _subscribers.erase(eventKey);
            }
        }

        template <typename TSubscriber, class TEvent>
        EXPORT void Unsubscribe(TSubscriber* instance, ClassConstEventHandlerFunction<TSubscriber, TEvent> callback)
        {
            Unsubscribe(instance, reinterpret_cast<ClassEventHandlerFunction<TSubscriber, TEvent>>(callback));
        }

        /// <summary>
        /// Immidiately processes an event and relays it to all subscribers.
        /// Returns true if the event was marked as handled, false otherwise.
        /// </summary>
        template <class TEvent>
        EXPORT bool Send(TEvent&& event)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            TBX_TRACE_VERBOSE("Sending the event \"{}\"", event.ToString());

            const auto hashCode = GetEventHash(event);
            std::lock_guard<std::mutex> lock(_mutex);
            if (!_subscribers.contains(hashCode))
            {
                return false;
            }

            const auto& callbacks = _subscribers[hashCode];
            for (auto& callback : callbacks)
            {
                if (EventSuppressor::IsSuppressing())
                {
                    TBX_TRACE("The event \"{}\" is suppressed", event.ToString());
                    return false;
                }
                callback.second(event);
            }

            return event.IsHandled;
        }

        /// <summary>
        /// Posts an event that will be processed and relayed to subscribers the next update.
        /// </summary>
        template <class TEvent>
        EXPORT void Post(TEvent&& event)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            TBX_TRACE_VERBOSE("Posting the event \"{}\"", event.ToString());

            std::lock_guard<std::mutex> lock(_mutex);
            using U = std::decay_t<TEvent>;
            _eventQueue.emplace(std::make_unique<U>(std::forward<TEvent>(event)));
        }

        /// <summary>
        /// Processes all queued events.
        /// </summary>
        EXPORT void ProcessQueue();

    private:
        template <class TEvent>
        EXPORT Tbx::uint64 GetEventHash() const
        {
            const auto& eventInfo = typeid(TEvent);
            const auto hash = eventInfo.hash_code();
            return static_cast<Tbx::uint64>(hash);
        }

        template <class TEvent>
        EXPORT Tbx::uint64 GetCallbackHash(EventHandlerFunction<TEvent> callback) const
        {
            const auto& callbackInfo = typeid(EventHandlerFunction<TEvent>);
            const auto hash = callbackInfo.hash_code() ^ reinterpret_cast<uintptr_t>(callback);
            return static_cast<Tbx::uint64>(hash);
        }

        template <class TEvent, typename TSubscriber>
        EXPORT Tbx::uint64 GetCallbackHash(TSubscriber* instance, ClassEventHandlerFunction<TSubscriber, TEvent> callback) const
        {
            const auto& callbackInfo = typeid(ClassEventHandlerFunction<TSubscriber, TEvent>);
            const auto hash = callbackInfo.hash_code() ^ reinterpret_cast<uintptr_t>(instance);
            return static_cast<Tbx::uint64>(hash);
        }

        Tbx::uint64 GetEventHash(const Event& event) const;
        Event& PopNextEventInQueue();

        std::unordered_map<Tbx::uint64, std::unordered_map<Tbx::uint64, EventCallback>> _subscribers = {};
        std::queue<Tbx::ExclusiveRef<Event>> _eventQueue = {};
        std::mutex _mutex = {};
        bool _hasPolled = false;
    };
}
