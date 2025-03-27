#include "Core/ToolboxPCH.h"
#include "Core/Event Dispatcher/EventDispatcher.h"

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