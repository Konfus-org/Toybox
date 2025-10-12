#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/Hashing.h"
#include "Tbx/Memory/Refs.h"
#include <unordered_map>
#include <queue>
#include <functional>
#include <atomic>

namespace Tbx
{
    /// <summary>
    /// Represents a function that consumes an <see cref="Event"/> instance.
    /// </summary>
    using EventCallback = std::function<void(Event&)>;

    using EventHash = uint64;

    class EventCarrier;

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

    /// <summary>
    /// Represents a bus for dispatching and receiving events.
    /// </summary>
    class TBX_EXPORT EventBus
    {
    public:
        using SubscriptionMap = std::unordered_map<EventHash, std::unordered_map<Uid, EventCallback>>;

        /// <summary>
        /// Creates an empty event bus.
        /// </summary>
        EventBus() = default;

        /// <summary>
        /// Destroys the event bus.
        /// </summary>
        ~EventBus();

        /// <summary>
        /// Processes all queued events, dispatching each to the relevant subscribers.
        /// </summary>
        void Flush();

        /// <summary>
        /// Public table mapping event types to registered subscribers.
        /// </summary>
        SubscriptionMap Subscriptions = {};

        /// <summary>
        /// Tracks which event type each subscription token belongs to.
        /// </summary>
        std::unordered_map<Uid, EventHash> SubscriptionIndex = {};

        /// <summary>
        /// Queue of events pending dispatch.
        /// </summary>
        std::queue<ExclusiveRef<Event>> EventQueue = {};

    private:
        friend class EventCarrier;
    };
}
