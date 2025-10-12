#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/EventBus.h"
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <mutex>

namespace Tbx
{
    /// <summary>
    /// Provides a lightweight façade over an <see cref="EventBus"/> for publishing events.
    /// </summary>
    class TBX_EXPORT EventDispatcher
    {
    public:
        EventDispatcher() = default;
        EventDispatcher(Ref<EventBus> eventBus);

        /// <summary>
        /// Associates the dispatcher with an event bus.
        /// </summary>
        void Bind(Ref<EventBus> eventBus);

        /// <summary>
        /// Determines whether the dispatcher is currently bound to a bus.
        /// </summary>
        bool IsBound() const;

        /// <summary>
        /// Publishes an event immediately to all subscribers.
        /// </summary>
        template <class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        bool Send(TEvent&& event)
        {
            auto bus = LockBus();
            if (bus == nullptr)
            {
                return false;
            }

            TBX_TRACE_VERBOSE("EventDispatcher: Sending the event \"{}\"", event.ToString());

            if (EventSuppressor::IsSuppressing())
            {
                TBX_TRACE_WARNING("EventDispatcher: The event \"{}\" is suppressed", event.ToString());
                return event.IsHandled;
            }

            const auto hashCode = bus->GetEventHash(event);

            std::unordered_map<Uid, EventCallback> callbacks;
            {
                std::scoped_lock lock(bus->_mutex);
                auto it = bus->_subscribers.find(hashCode);
                if (it == bus->_subscribers.end())
                {
                    return event.IsHandled;
                }

                callbacks = it->second;
            }

            for (auto& [id, cb] : callbacks)
            {
                if (EventSuppressor::IsSuppressing())
                {
                    TBX_TRACE_WARNING("EventDispatcher: The event \"{}\" is suppressed during dispatch", event.ToString());
                    return event.IsHandled;
                }

                cb(event);
            }

            return event.IsHandled;
        }

        /// <summary>
        /// Queues an event for delivery during the next call to <see cref="EventBus::Flush"/>.
        /// </summary>
        template <class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        void Post(TEvent event)
        {
            auto bus = LockBus();
            if (bus == nullptr)
            {
                return;
            }

            TBX_TRACE_VERBOSE("EventDispatcher: Posting the event \"{}\"", event.ToString());

            std::scoped_lock lock(bus->_mutex);
            bus->_eventQueue.emplace(MakeExclusive<std::decay_t<TEvent>>(std::move(event)));
        }

        /// <summary>
        /// Retrieves the bound event bus.
        /// </summary>
        Ref<EventBus> GetEventBus() const;

    private:
        Ref<EventBus> LockBus() const;

    private:
        WeakRef<EventBus> _eventBus = {};
    };
}
