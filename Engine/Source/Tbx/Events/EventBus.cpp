#include "Tbx/PCH.h"
#include "Tbx/Events/EventBus.h"

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

    void EventBus::Flush()
    {
        // Pull all queued events out under lock to minimize lock-hold time while dispatching.
        std::queue<ExclusiveRef<Event>> localQueue;
        {
            std::scoped_lock lock(_mutex);
            localQueue.swap(_eventQueue);
        }

        while (!localQueue.empty())
        {
            auto evt = std::move(localQueue.front());
            localQueue.pop();
            if (!evt) continue;

            // If suppressed globally, skip processing of queued events as well.
            if (EventSuppressor::IsSuppressing())
            {
                TBX_TRACE_WARNING("EventBus: Queued event \"{}\" suppressed", evt->ToString());
                continue;
            }

            SendEvent(*evt);
        }
    }

    void EventBus::AddSubscriber(EventHash eventKey, const Uid& token, EventCallback callable)
    {
        std::scoped_lock lock(_mutex);
        auto& callbacks = _subscribers[eventKey];
        callbacks[token] = std::move(callable);
        _subscriptionIndex[token] = eventKey;
    }

    void EventBus::RemoveSubscriber(const Uid& token)
    {
        std::scoped_lock lock(_mutex);
        auto tokenIt = _subscriptionIndex.find(token);
        if (tokenIt == _subscriptionIndex.end())
        {
            return;
        }

        const auto eventKey = tokenIt->second;
        auto eventIt = _subscribers.find(eventKey);
        if (eventIt == _subscribers.end())
        {
            _subscriptionIndex.erase(tokenIt);
            return;
        }

        auto& callbacks = eventIt->second;
        callbacks.erase(token);
        if (callbacks.empty())
        {
            _subscribers.erase(eventIt);
        }

        _subscriptionIndex.erase(tokenIt);
    }

    void EventBus::Unsubscribe(const Uid& token)
    {
        RemoveSubscriber(token);
    }

    void EventBus::SendEvent(Event& event)
    {
        TBX_TRACE_VERBOSE("EventBus: Dispatching the event \"{}\"", event.ToString());

        if (EventSuppressor::IsSuppressing())
        {
            TBX_TRACE_WARNING("EventBus: The event \"{}\" is suppressed", event.ToString());
            return;
        }

        const auto hashCode = GetEventHash(event);

        // copy callbacks out under lock to avoid holding lock while calling subscribers
        std::unordered_map<Uid, EventCallback> callbacks;
        {
            std::scoped_lock lock(_mutex);
            auto it = _subscribers.find(hashCode);
            if (it == _subscribers.end())
            {
                return;
            }
            callbacks = it->second; // copy
        }

        for (auto& [id, cb] : callbacks)
        {
            // check suppress again in case a subscriber suppressed events
            if (EventSuppressor::IsSuppressing())
            {
                TBX_TRACE_WARNING("EventBus: The event \"{}\" is suppressed during dispatch", event.ToString());
                return;
            }
            cb(event);
        }
    }

    EventHash EventBus::GetEventHash(const Event& event) const
    {
        const auto& eventInfo = typeid(event);
        const auto hash = eventInfo.hash_code();
        return static_cast<EventHash>(hash);
    }

    ExclusiveRef<Event> EventBus::PopNextEventInQueue()
    {
        std::scoped_lock lock(_mutex);
        if (_eventQueue.empty()) return nullptr;

        auto evt = std::move(_eventQueue.front());
        _eventQueue.pop();
        return evt;
    }
}
