#include "Tbx/PCH.h"
#include "Tbx/Events/EventCoordinator.h"

namespace Tbx 
{
    //////////// Event Suppressor ///////////////

    std::atomic_int EventSuppressor::_suppressCount = 0;

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
        return _suppressCount.load(std::memory_order_relaxed) > 0;
    }

    void EventSuppressor::Suppress()
    {
        _suppressCount.fetch_add(1, std::memory_order_relaxed);
    }

    void EventSuppressor::Unsuppress()
    {
        _suppressCount.fetch_sub(1, std::memory_order_relaxed);
    }

    //////////// Event Coordinator ///////////////

    std::unordered_map<std::size_t, std::unordered_map<std::size_t, EventCallback>> EventCoordinator::_subscribers = {};
    std::mutex EventCoordinator::_subscribersMutex;

    void EventCoordinator::ClearSubscribers()
    {
        std::lock_guard<std::mutex> lock(_subscribersMutex);
        _subscribers.clear();
    }

    std::unordered_map<std::size_t, std::unordered_map<std::size_t, EventCallback>>& EventCoordinator::GetSubscribers()
    {
        return _subscribers;
    }

    std::mutex& EventCoordinator::GetMutex()
    {
        return _subscribersMutex;
    }
}
