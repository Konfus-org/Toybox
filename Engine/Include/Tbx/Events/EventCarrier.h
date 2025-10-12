#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventSync.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Memory/Refs.h"
#include <type_traits>
#include <unordered_map>

namespace Tbx
{
    class TBX_EXPORT EventCarrier
    {
    public:
        EventCarrier() = default;
        explicit EventCarrier(Ref<EventBus> bus);
        ~EventCarrier() = default;

        EventCarrier(const EventCarrier&) = default;
        EventCarrier& operator=(const EventCarrier&) = default;
        EventCarrier(EventCarrier&&) noexcept = default;
        EventCarrier& operator=(EventCarrier&&) noexcept = default;

        /// <summary>
        /// Immediately broadcasts an event to the registered listeners.
        /// </summary>
        /// <typeparam name="TEvent">The event type being dispatched.</typeparam>
        /// <param name="event">The event instance to send.</param>
        /// <returns><c>true</c> if the event was marked as handled; otherwise, <c>false</c>.</returns>
        template <class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        bool Send(TEvent&& event)
        {
            TBX_TRACE_VERBOSE("Event Carrier: Sending the event \"{}\"", event.ToString());

            TBX_ASSERT(_bus, "EventCarrier: Cannot send without a valid event bus.");
            if (!_bus)
            {
                return event.IsHandled;
            }

            std::unordered_map<Uid, EventCallback> callbacks;
            const auto hashCode = Memory::Hash(event);

            {
                EventSync sync;
                auto it = _bus->Subscriptions.find(hashCode);
                if (it == _bus->Subscriptions.end())
                {
                    return event.IsHandled;
                }

                callbacks = it->second;
            }

            for (auto& [id, cb] : callbacks)
            {
                if (EventSuppressor::IsSuppressing())
                {
                    TBX_TRACE_WARNING("EventCarrier: The event \"{}\" is suppressed during send", event.ToString());
                    return event.IsHandled;
                }

                cb(event);
            }
            return event.IsHandled;
        }

        /// <summary>
        /// Queues an event to be delivered when the bus is flushed.
        /// </summary>
        /// <typeparam name="TEvent">The event type being queued.</typeparam>
        /// <param name="event">The event instance to enqueue.</param>
        template <class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        void Post(TEvent event)
        {
            TBX_TRACE_VERBOSE("Event Carrier: Posting the event \"{}\"", event.ToString());

            TBX_ASSERT(_bus, "EventCarrier: Cannot queue without a valid event bus.");
            if (!_bus)
            {
                return;
            }

            EventSync sync;
            _bus->EventQueue.emplace(MakeExclusive<std::decay_t<TEvent>>(std::move(event)));
        }

    private:
        Ref<EventBus> _bus = nullptr;
    };
}

