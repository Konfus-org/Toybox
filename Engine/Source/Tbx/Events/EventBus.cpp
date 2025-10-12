#include "Tbx/PCH.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Events/EventSync.h"
#include <unordered_map>

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

    EventBus::~EventBus()
    {
        EventSync sync;
        while (!EventQueue.empty())
        {
            EventQueue.pop();
        }
        Subscriptions.clear();
        SubscriptionIndex.clear();
    }

    void EventBus::Flush()
    {
        std::queue<ExclusiveRef<Event>> localQueue;
        {
            EventSync sync;
            localQueue.swap(EventQueue);
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

            std::unordered_map<Uid, EventCallback> callbacks;
            const auto hashCode = Memory::Hash(*evt);

            {
                EventSync sync;
                auto it = Subscriptions.find(hashCode);
                if (it == Subscriptions.end())
                {
                    continue;
                }

                callbacks = it->second;
            }

            for (auto& [id, cb] : callbacks)
            {
                if (EventSuppressor::IsSuppressing())
                {
                    TBX_TRACE_WARNING("EventBus: The event \"{}\" is suppressed during flush", evt->ToString());
                    break;
                }

                cb(*evt);
            }
        }
    }
}
