#include "Tbx/PCH.h"
#include "Tbx/Events/EventCoordinator.h"

namespace Tbx 
{
    //////////// Event Suppressor ///////////////

    int EventSuppressor::_suppressCount = 0;

    EventSuppressor::EventSuppressor()
    {
        Suppress();
    }

    EventSuppressor::~EventSuppressor()
    {
        Unsuppress();
    }

    bool EventSuppressor::IsSuppressing()
    {
        return _suppressCount > 0;
    }

    void EventSuppressor::Suppress()
    {
        _suppressCount++;
    }

    void EventSuppressor::Unsuppress()
    {
        _suppressCount--;
    }

    //////////// Event Coordinator ///////////////

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