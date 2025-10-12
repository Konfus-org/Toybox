#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/EventBus.h"
#include <type_traits>
#include <utility>

namespace Tbx
{
    /// <summary>
    /// Provides a lightweight façade over an <see cref="EventBus"/> for publishing events.
    /// </summary>
    class TBX_EXPORT EventDispatcher
    {
    public:
        EventDispatcher() = default;
        explicit EventDispatcher(Ref<EventBus> eventBus);

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

            return bus->Send(std::forward<TEvent>(event));
        }

        /// <summary>
        /// Queues an event for delivery during the next flush.
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

            bus->Post(std::move(event));
        }

        /// <summary>
        /// Dispatches any queued events.
        /// </summary>
        void Flush();

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
