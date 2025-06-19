#include "Tbx/PCH.h"
#include "Tbx/Events/EventCoordinator.h"

namespace Tbx 
{
    std::unordered_map<hash, std::vector<Callback<Event>>> EventCoordinator::_subscribers = {};

    void EventCoordinator::ClearSubscribers()
    {
        _subscribers.clear();
    }

    std::unordered_map<hash, std::vector<Callback<Event>>>& EventCoordinator::GetSubscribers()
    {
        return _subscribers;
    }
}