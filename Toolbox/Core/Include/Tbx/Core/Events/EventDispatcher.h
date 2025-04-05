#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Events/Event.h"
#include "Tbx/Core/Callbacks/CallbackAPI.h"
#include "Tbx/Core/Debug/DebugAPI.h"
#include <unordered_map>
#include <typeindex>
#include <vector>

#define TBX_BIND_CALLBACK(fn) [this](auto&&... args) { return this->fn(std::forward<decltype(args)>(args)...); }
#define TBX_BIND_STATIC_CALLBACK(fn) [](auto&&... args) { return fn(std::forward<decltype(args)>(args)...); }

namespace Tbx
{
    class EventDispatcher
    {
    public:
        template <class TEvent>
        EXPORT static inline UID Subscribe(const CallbackFunction<TEvent>& callback)
        {
            const auto& eventInfo = typeid(TEvent);
            const auto& hashCode = eventInfo.hash_code();

            if (GetSubscribers().contains(hashCode) == false)
            {
                GetSubscribers()[hashCode] = std::vector<Callback<Event>>();
            }

            auto& callbacks = GetSubscribers()[hashCode];
            auto callbackToAdd = Callback<Event>([callback](Event& event) { callback(static_cast<TEvent&>(event)); });
            callbacks.push_back(callbackToAdd);

            return callbackToAdd.GetId();
        }

        template <class TEvent>
        EXPORT static void Unsubscribe(const UID& callbackToUnsub)
        {
            const auto& eventInfo = typeid(TEvent);
            const auto& hashCode = eventInfo.hash_code();

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

        template <class TEvent>
        EXPORT static inline bool Dispatch(TEvent& event)
        {
            const auto& eventInfo = typeid(TEvent);
            const auto& hashCode = eventInfo.hash_code();

            if (GetSubscribers().contains(hashCode) == false)
            {
                return false;
            }

            const auto& callbacks = GetSubscribers()[hashCode];
            for (auto& callback : callbacks)
            {
                callback(event);
            }
            
            return event.IsHandled;
        }

        EXPORT static void Clear();
        EXPORT static std::unordered_map<hash, std::vector<Callback<Event>>>& GetSubscribers();

    private:
        static std::unordered_map<hash, std::vector<Callback<Event>>> _subscribers;
    };
}