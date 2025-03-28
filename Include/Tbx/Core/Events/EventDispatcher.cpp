#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Events/EventDispatcher.h"

namespace Tbx 
{
    std::unordered_map<hash, std::vector<Callback<Event>>> EventDispatcher::_subscribers;
    std::mutex EventDispatcher::_mutex;

    void EventDispatcher::Clear()
    {
        _subscribers.clear();
    }

    std::mutex& EventDispatcher::GetMutex()
    {
        return _mutex;
    }

    std::unordered_map<hash, std::vector<Callback<Event>>>& EventDispatcher::GetSubscribers()
    {
        return _subscribers;
    }
}