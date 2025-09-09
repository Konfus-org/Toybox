#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Callbacks/CallbackFunction.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Events/Event.h"
#include <unordered_map>
#include <typeindex>
#include <vector>
#include <mutex>
#include <atomic>

namespace Tbx
{
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

    using EventCallback = CallbackFunction<Event>;

    class EventCoordinator
    {
    public:
        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// The function name is used as the key so no UID needs to be tracked.
        /// For member functions pass the instance and the method pointer. For free or static
        /// functions just pass the function pointer.
        /// </summary>
        template <class TEvent>
        EXPORT static void Subscribe(void (*callback)(TEvent&))
        {
            const auto& eventInfo = typeid(TEvent);
            const auto hashCode = eventInfo.hash_code();

            std::lock_guard<std::mutex> lock(GetMutex());
            if (GetSubscribers().contains(hashCode) == false)
            {
                GetSubscribers()[hashCode] = {};
            }

            auto key = MakeKey(callback);
            GetSubscribers()[hashCode][key] = [callback](Event& event) { callback(static_cast<TEvent&>(event)); };
        }

        template <class TEvent, typename T>
        EXPORT static void Subscribe(T* instance, void (T::*callback)(TEvent&))
        {
            const auto& eventInfo = typeid(TEvent);
            const auto hashCode = eventInfo.hash_code();

            std::lock_guard<std::mutex> lock(GetMutex());
            if (GetSubscribers().contains(hashCode) == false)
            {
                GetSubscribers()[hashCode] = {};
            }

            auto key = MakeKey(instance, callback);
            GetSubscribers()[hashCode][key] = [instance, callback](Event& event) { (instance->*callback)(static_cast<TEvent&>(event)); };
        }

        /// <summary>
        /// Removes the method associated with the given function from the list of callbacks for an event.
        /// </summary>
        template <class TEvent>
        EXPORT static void Unsubscribe(void (*callback)(TEvent&))
        {
            const auto& eventInfo = typeid(TEvent);
            const auto hashCode = eventInfo.hash_code();

            std::lock_guard<std::mutex> lock(GetMutex());
            if (GetSubscribers().contains(hashCode) == false)
            {
                return;
            }

            auto key = MakeKey(callback);
            auto& callbacks = GetSubscribers()[hashCode];
            if (callbacks.erase(key) == 0)
            {
                TBX_ASSERT(false, "Failed to unsubscribe from event {}. Callback not found!", eventInfo.name());
            }
            if (callbacks.empty())
            {
                GetSubscribers().erase(hashCode);
            }
        }

        template <class TEvent, typename T>
        EXPORT static void Unsubscribe(T* instance, void (T::*callback)(TEvent&))
        {
            const auto& eventInfo = typeid(TEvent);
            const auto hashCode = eventInfo.hash_code();

            std::lock_guard<std::mutex> lock(GetMutex());
            if (GetSubscribers().contains(hashCode) == false)
            {
                return;
            }

            auto key = MakeKey(instance, callback);
            auto& callbacks = GetSubscribers()[hashCode];
            if (callbacks.erase(key) == 0)
            {
                TBX_ASSERT(false, "Failed to unsubscribe from event {}. Callback not found!", eventInfo.name());
            }
            if (callbacks.empty())
            {
                GetSubscribers().erase(hashCode);
            }
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
        EXPORT static std::unordered_map<std::size_t, std::unordered_map<std::size_t, EventCallback>>& GetSubscribers();
        EXPORT static std::mutex& GetMutex();

        static std::unordered_map<std::size_t, std::unordered_map<std::size_t, EventCallback>> _subscribers;
        static std::mutex _subscribersMutex;

        template <class TEvent>
        static std::size_t MakeKey(void (*callback)(TEvent&))
        {
            return std::hash<void (*)(TEvent&)>()(callback);
        }

        template <class TEvent, typename T>
        static std::size_t MakeKey(T* instance, void (T::*callback)(TEvent&))
        {
            return std::hash<void (T::*)(TEvent&)>()(callback) ^ reinterpret_cast<std::size_t>(instance);
        }
    };
}
