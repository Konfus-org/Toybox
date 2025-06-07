#include "Tbx/Systems/PCH.h"
#include "Tbx/Systems/Events/EventCoordinator.h"

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