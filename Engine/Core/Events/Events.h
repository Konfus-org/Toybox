#pragma once
#include "TbxAPI.h"
#include "TbxPCH.h"
#include "Event.h"
#include "Callback.h"
#include "KeyEvents.h"
#include "MouseEvents.h"
#include "WindowEvents.h"
#include "ApplicationEvents.h"
#include "RenderEvents.h"
#include "LogEvents.h"
#include <typeindex>
#include <mutex>

#define TBX_BIND_CALLBACK(fn) [this](auto&&... args) { return this->fn(std::forward<decltype(args)>(args)...); }

namespace Tbx
{
    class Events
    {
    public:
        template <class TEvent>
        TBX_API static inline UUID Subscribe(const CallbackFunction<TEvent>& callback)
        {
            std::scoped_lock<std::mutex> lock(_mutex);

            const auto& eventInfo = typeid(TEvent);
            const auto& hashCode = eventInfo.hash_code();

            if (_subscribers.contains(hashCode) == false)
            {
                _subscribers[hashCode] = std::vector<Callback<Event>>();
            }

            auto& callbacks = _subscribers[hashCode];
            auto callbackToAdd = Callback<Event>([callback](Event& event) { callback(static_cast<TEvent&>(event)); });
            callbacks.push_back(callbackToAdd);

            return callbackToAdd.GetId();
        }

        TBX_API static inline void Unsubscribe(const UUID& callbackToUnsub)
        {
            std::scoped_lock<std::mutex> lock(_mutex);

            for (auto& [hashCode, callbacks] : _subscribers)
            {
                for (auto it = callbacks.begin(); it != callbacks.end();)
                {
                    if (it->GetId() != callbackToUnsub)
                    {
                        it++;
                        continue;
                    }

                    it = callbacks.erase(it);
                    if (callbacks.empty()) _subscribers.erase(hashCode);
                    return;
                }
            }
        }

        template <class TEvent>
        TBX_API static inline bool Send(TEvent& event)
        {
            std::scoped_lock<std::mutex> lock(_mutex);

            const auto& eventInfo = typeid(TEvent);
            const auto& hashCode = eventInfo.hash_code();

            if (_subscribers.contains(hashCode) == false)
            {
                return false;
            }

            const auto& callbacks = _subscribers[hashCode];
            for (auto& callback : callbacks)
            {
                callback(event);
            }
            
            return event.Handled;
        }

    private:
        static inline std::unordered_map<hash, std::vector<Callback<Event>>> _subscribers;
        static inline std::mutex _mutex;
    };
}