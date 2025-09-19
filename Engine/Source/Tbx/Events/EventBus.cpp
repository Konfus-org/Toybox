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

    void EventBus::ProcessQueue()
    {
        TBX_TRACE_VERBOSE("Polling events...");

        while (true)
        {
            auto nextEvent = PopNextEventInQueue();
            if (!nextEvent)
            {
                break;
            }

            Send(*nextEvent);
        }

        TBX_TRACE_VERBOSE("Finished polling events...");
    }

    std::unique_ptr<Event> EventBus::PopNextEventInQueue()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_eventQueue.empty())
        {
            return nullptr;
        }

        auto next = std::move(_eventQueue.front());
        _eventQueue.pop();
        return next;
    }

    Tbx::uint64 EventBus::GetEventHash(const Event& event) const
    {
        const auto& eventInfo = typeid(event);
        const auto hash = eventInfo.hash_code();
        return static_cast<Tbx::uint64>(hash);
    }
}
