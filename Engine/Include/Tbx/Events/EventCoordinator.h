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
    /// Events will be suppressed during the lifetime of this class.
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
    class EventCoordinator
    {
    public:
        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// </summary>
        template <class TEvent>
        EXPORT static void Subscribe(EventHandlerFunction<TEvent> callback)
        {
            auto eventKey = GetEventHash<TEvent>();
            auto callbackKey = GetCallbackHash(callback);

            std::lock_guard<std::mutex> lock(GetMutex());
            if (GetSubscribers().contains(eventKey) == false)
            {
                GetSubscribers()[eventKey] = {};
            }

            GetSubscribers()[eventKey][callbackKey] =
                [callback](Event& event) { callback(static_cast<TEvent&>(event)); };
        }

        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// </summary>
        template <class TEvent>
        EXPORT static void Subscribe(ConstEventHandlerFunction<TEvent> callback)
        {
            Subscribe(reinterpret_cast<EventHandlerFunction<TEvent>>(callback));
        }

        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// </summary>
        template <typename T, class TEvent>
        EXPORT static void Subscribe(T* instance, ClassEventHandlerFunction<T, TEvent> callback)
        {
            auto eventKey = GetEventHash<TEvent>();
            auto callbackKey = GetCallbackHash(instance, callback);

            std::lock_guard<std::mutex> lock(GetMutex());
            if (GetSubscribers().contains(eventKey) == false)
            {
                GetSubscribers()[eventKey] = {};
            }

            GetSubscribers()[eventKey][callbackKey] =
                [instance, callback](Event& event) { (instance->*callback)(static_cast<TEvent&>(event)); };
        }

        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// </summary>
        template <typename T, class TEvent>
        EXPORT static void Subscribe(T* instance, ClassConstEventHandlerFunction<T, TEvent> callback)
        {
            Subscribe(instance, reinterpret_cast<ClassEventHandlerFunction<T, TEvent>>(callback));
        }

        /// <summary>
        /// Removes the method associated with the given function from the list of callbacks for an event.
        /// </summary>
        template <class TEvent>
        EXPORT static void Unsubscribe(EventHandlerFunction<TEvent> callback)
        {
            auto eventKey = GetEventHash<TEvent>();

            std::lock_guard<std::mutex> lock(GetMutex());
            if (GetSubscribers().contains(eventKey) == false)
            {
                return;
            }

            auto& callbacks = GetSubscribers()[eventKey];
            auto callbackKey = GetCallbackHash(callback);

            if (callbacks.erase(callbackKey) == 0)
            {
                const auto& eventInfo = typeid(TEvent);
                TBX_ASSERT(false, "Failed to unsubscribe from event. Callback not found!", eventInfo.name());
            }

            if (callbacks.empty())
            {
                GetSubscribers().erase(eventKey);
            }
        }

        template <class TEvent>
        EXPORT static void Unsubscribe(ConstEventHandlerFunction<TEvent> callback)
        {
            Unsubscribe(reinterpret_cast<EventHandlerFunction<TEvent>>(callback));
        }

        template <class TEvent, typename T>
        EXPORT static void Unsubscribe(T* instance, ClassEventHandlerFunction<T, TEvent> callback)
        {
            auto eventKey = GetEventHash<TEvent>();

            std::lock_guard<std::mutex> lock(GetMutex());
            if (GetSubscribers().contains(eventKey) == false)
            {
                return;
            }

            auto callbackKey = GetCallbackHash(instance, callback);
            auto& callbacks = GetSubscribers()[eventKey];

            if (callbacks.erase(callbackKey) == 0)
            {
                const auto& eventInfo = typeid(TEvent);
                TBX_ASSERT(false, "Failed to unsubscribe from event {}. Callback not found!", eventInfo.name());
            }

            if (callbacks.empty())
            {
                GetSubscribers().erase(eventKey);
            }
        }

        template <class TEvent, typename T>
        EXPORT static void Unsubscribe(T* instance, ClassConstEventHandlerFunction<T, TEvent> callback)
        {
            Unsubscribe(instance, reinterpret_cast<ClassEventHandlerFunction<T, TEvent>>(callback));
        }

        /// <summary>
        /// Sends an event to all subscribers.
        /// </summary>
        template <class TEvent>
        EXPORT static bool Send(TEvent& event)
        {
            const auto& eventInfo = typeid(TEvent);
            const auto hashCode = eventInfo.hash_code();

            std::vector<EventCallback> callbacksCopy;
            {
                std::lock_guard<std::mutex> lock(GetMutex());
                if (GetSubscribers().contains(hashCode) == false)
                {
                    return false;
                }
                auto mapCopy = GetSubscribers()[hashCode];
                callbacksCopy.reserve(mapCopy.size());
                for (auto& [_, cb] : mapCopy)
                {
                    callbacksCopy.push_back(cb);
                }
            }

            for (auto& callback : callbacksCopy)
            {
                if (EventSuppressor::IsSuppressing())
                {
                    TBX_TRACE("Event {} suppressed", eventInfo.name());
                    return false;
                }

                callback(event);
            }

            return event.IsHandled;
        }

        /// <summary>
        /// Clears all subscribers for all events.
        /// </summary>
        EXPORT static void ClearSubscribers();

    private:
        template <class TEvent>
        EXPORT static Tbx::uint64 GetEventHash()
        {
            const auto& eventInfo = typeid(TEvent);
            const auto hash = eventInfo.hash_code();
            return static_cast<Tbx::uint64>(hash);
        }

        template <class TEvent>
        EXPORT static Tbx::uint64 GetCallbackHash(EventHandlerFunction<TEvent> callback)
        {
            const auto& callbackInfo = typeid(EventHandlerFunction<TEvent>);
            const auto hash = callbackInfo.hash_code() ^ reinterpret_cast<uintptr_t>(callback);
            return static_cast<Tbx::uint64>(hash);
        }

        template <typename T, class TEvent>
        EXPORT static Tbx::uint64 GetCallbackHash(T* instance, ClassEventHandlerFunction<T, TEvent> callback)
        {
            const auto& callbackInfo = typeid(ClassEventHandlerFunction<T, TEvent>);
            const auto hash = callbackInfo.hash_code() ^ reinterpret_cast<uintptr_t>(instance);
            return static_cast<Tbx::uint64>(hash);
        }

        EXPORT static std::unordered_map<std::size_t, std::unordered_map<std::size_t, EventCallback>>& GetSubscribers();
        EXPORT static std::mutex& GetMutex();

        static std::unordered_map<std::size_t, std::unordered_map<std::size_t, EventCallback>> _subscribers;
        static std::mutex _subscribersMutex;
    };
}
