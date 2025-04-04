#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Events/EventDispatcher.h"

namespace Tbx 
{
    std::unordered_map<hash, std::vector<Callback<Event>>> EventDispatcher::_subscribers = {};

    void EventDispatcher::Unsubscribe(const UID& callbackToUnsub)
    {

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

    void EventDispatcher::Clear()
    {
        _subscribers.clear();
    }

    std::unordered_map<hash, std::vector<Callback<Event>>>& EventDispatcher::GetSubscribers()
    {
        return _subscribers;
    }
}