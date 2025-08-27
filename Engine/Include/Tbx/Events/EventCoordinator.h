#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Callbacks/Callback.h"
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

    class EventCoordinator
    {
    public:
        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// If passing a classes function you must first bind it to the callback like using TBX_BIND_FN or 
        /// if the function is static or not associated with a class instance use TBX_BIND_STATIC_FN.
        /// The UID returned is the ID of the callback and can be used to unsubscribe from the event.
        /// </summary>
        template <class TEvent>
        EXPORT static Uid Subscribe(const CallbackFunction<TEvent>& callback)
        {
            const auto& eventInfo = typeid(TEvent);
            const auto hashCode = eventInfo.hash_code();

            std::lock_guard<std::mutex> lock(GetMutex());
            if (GetSubscribers().contains(hashCode) == false)
            {
                GetSubscribers()[hashCode] = std::vector<Callback<Event>>();
            }

            auto& callbacks = GetSubscribers()[hashCode];
            const Callback<Event>& newCallback = callbacks
                .emplace_back([callback](Event& event) { callback(static_cast<TEvent&>(event)); });

            return newCallback.GetId();
        }

        /// <summary>
        /// Removes the method associated with the given UID from the list of callbacks for an event.
        /// </summary>
        template <class TEvent>
        EXPORT static void Unsubscribe(const Uid& callbackToUnsub)
        {
            const auto& eventInfo = typeid(TEvent);
            const auto hashCode = eventInfo.hash_code();

            std::lock_guard<std::mutex> lock(GetMutex());
            if (GetSubscribers().contains(hashCode) == false)
            {
                return;
            }

            auto& callbacks = GetSubscribers()[hashCode];
            auto callbackToDeleteIt = std::find_if(callbacks.begin(), callbacks.end(), [callbackToUnsub](const Callback<Event>& c)
            {
                return c.GetId() == callbackToUnsub;
            });

            if (callbackToDeleteIt != callbacks.end())
            {
                callbacks.erase(callbackToDeleteIt);
                if (callbacks.empty())
                {
                    GetSubscribers().erase(hashCode);
                }
            }
            else
            {
                TBX_ASSERT(false, "Failed to unsubscribe from event {}. Callback not found!", eventInfo.name());
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

            std::vector<Callback<Event>> callbacksCopy;
            {
                std::lock_guard<std::mutex> lock(GetMutex());
                if (GetSubscribers().contains(hashCode) == false)
                {
                    return false;
                }
                callbacksCopy = GetSubscribers()[hashCode];
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
        EXPORT static std::unordered_map<hash, std::vector<Callback<Event>>>& GetSubscribers();
        EXPORT static std::mutex& GetMutex();

        static std::unordered_map<hash, std::vector<Callback<Event>>> _subscribers;
        static std::mutex _subscribersMutex;
    };
}
