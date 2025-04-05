#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Events/EventDispatcher.h"

namespace Tbx 
{
    std::unordered_map<hash, std::vector<Callback<Event>>> EventDispatcher::_subscribers = {};

    void EventDispatcher::Clear()
    {
        _subscribers.clear();
    }

    std::unordered_map<hash, std::vector<Callback<Event>>>& EventDispatcher::GetSubscribers()
    {
        return _subscribers;
    }
}