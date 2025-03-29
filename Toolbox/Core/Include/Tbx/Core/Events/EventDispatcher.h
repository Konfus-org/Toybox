#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Events/Event.h"
#include "Tbx/Core/Callbacks/CallbackAPI.h"
#include <typeindex>
#include <mutex>

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
            std::scoped_lock<std::mutex> lock(GetMutex());

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

        EXPORT static inline void Unsubscribe(const UID& callbackToUnsub)
        {
            std::scoped_lock<std::mutex> lock(GetMutex());

            for (auto& [hashCode, callbacks] : GetSubscribers())
            {
                for (auto it = callbacks.begin(); it != callbacks.end();)
                {
                    if (it->GetId() != callbackToUnsub)
                    {
                        it++;
                        continue;
                    }

                    it = callbacks.erase(it);
                    if (callbacks.empty()) GetSubscribers().erase(hashCode);
                    return;
                }
            }
        }

        template <class TEvent>
        EXPORT static inline bool Send(TEvent& event)
        {
            std::scoped_lock<std::mutex> lock(GetMutex());

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
        static std::mutex _mutex;

        EXPORT static std::mutex& GetMutex();
    };
}