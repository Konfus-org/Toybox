#pragma once
#include "Tbx/Debug/Debugging.h"
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/Refs.h"
#include <unordered_map>
#include <queue>
#include <functional>
#include <atomic>
#include <mutex>
#include <type_traits>
#include <utility>

namespace Tbx
{
    /// <summary>
    /// Represents a function that consumes an <see cref="Event"/> instance.
    /// </summary>
    using EventCallback = std::function<void(Event&)>;

    /// <summary>
    /// ALL events on ALL event busses will be suppressed during the lifetime of this class.
    /// Use with care.
    /// </summary>
    class TBX_EXPORT EventSuppressor
    {
    public:
        /// <summary>
        /// Increments the global suppression counter, preventing events from being processed.
        /// </summary>
        EventSuppressor();

        /// <summary>
        /// Decrements the global suppression counter, restoring normal event processing when the counter reaches zero.
        /// </summary>
        ~EventSuppressor();

        /// <summary>
        /// Indicates whether events are currently being suppressed across all buses.
        /// </summary>
        /// <returns><c>true</c> when dispatch is disabled; otherwise, <c>false</c>.</returns>
        static bool IsSuppressing();

    private:
        /// <summary>
        /// Increases the suppression counter.
        /// </summary>
        static void Suppress();

        /// <summary>
        /// Decreases the suppression counter.
        /// </summary>
        static void Unsuppress();

    private:
        static std::atomic_int _suppressCount;
    };

    class TBX_EXPORT EventBus
    {
    public:
        /// <summary>
        /// Creates an empty event bus.
        /// </summary>
        EventBus() = default;

        /// <summary>
        /// Destroys the event bus.
        /// </summary>
        ~EventBus() = default;

        using EventHash = uint64;

        /// <summary>
        /// Registers a callback for the specified event type.
        /// </summary>
        /// <typeparam name="TEvent">The event type to receive.</typeparam>
        /// <param name="callback">The callback invoked when the event is dispatched.</param>
        /// <returns>A unique subscription token that can later be supplied to <see cref="Unsubscribe"/>.</returns>
        template <class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        Uid Subscribe(EventCallback callback)
        {
            TBX_ASSERT(callback, "EventBus: Cannot subscribe an empty callback.");

            const auto eventKey = GetEventHash<TEvent>();
            const auto token = Uid::Generate();
            AddSubscriber(eventKey, token, std::move(callback));
            return token;
        }

        /// <summary>
        /// Removes the subscription represented by the provided token.
        /// </summary>
        /// <param name="token">The subscription token returned during registration.</param>
        void Unsubscribe(const Uid& token);

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
            TBX_TRACE_VERBOSE("Event Bus: Sending the event \"{}\"", event.ToString());
            SendEvent(event);
            return event.IsHandled;
        }

        /// <summary>
        /// Queues an event to be delivered during the next call to <see cref="ProcessQueue"/>.
        /// </summary>
        /// <typeparam name="TEvent">The event type being queued.</typeparam>
        /// <param name="event">The event instance to enqueue.</param>
        template <class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        void Post(TEvent event)
        {
            TBX_TRACE_VERBOSE("Event Bus: Posting the event \"{}\"", event.ToString());
            std::scoped_lock lock(_mutex);
            _eventQueue.emplace(std::make_unique<std::decay_t<TEvent>>(std::move(event)));
        }

        /// <summary>
        /// Processes all queued events, dispatching each to the relevant subscribers.
        /// </summary>
        void ProcessQueue();

    private:
        /// <summary>
        /// Adds a new subscriber to the internal lookup tables.
        /// </summary>
        /// <param name="eventKey">The hashed event type key.</param>
        /// <param name="token">The subscription token.</param>
        /// <param name="callable">The callback to invoke.</param>
        void AddSubscriber(EventHash eventKey, const Uid& token, EventCallback callable);

        /// <summary>
        /// Removes the subscriber referenced by the supplied token.
        /// </summary>
        /// <param name="token">The subscription token.</param>
        void RemoveSubscriber(const Uid& token);

        /// <summary>
        /// Dispatches a single event to its listeners.
        /// </summary>
        /// <param name="event">The event to send.</param>
        void SendEvent(Event& event);

        /// <summary>
        /// Retrieves the next queued event, if one exists.
        /// </summary>
        /// <returns>The next event in the queue, or an empty reference when none remain.</returns>
        ExclusiveRef<Event> PopNextEventInQueue();

        /// <summary>
        /// Computes the hash representing the supplied event instance.
        /// </summary>
        /// <param name="event">The event used to derive the hash.</param>
        /// <returns>The hash associated with the event type.</returns>
        EventHash GetEventHash(const Event& event) const;

        template <class TEvent>
        EventHash GetEventHash() const
        {
            const auto& eventInfo = typeid(TEvent);
            const auto hash = eventInfo.hash_code();
            return static_cast<EventHash>(hash);
        }

    private:
        mutable std::mutex _mutex = {};
        std::unordered_map<EventHash, std::unordered_map<Uid, EventCallback>> _subscribers = {};
        std::unordered_map<Uid, EventHash> _subscriptionIndex = {};
        std::queue<ExclusiveRef<Event>> _eventQueue = {};
    };
}
