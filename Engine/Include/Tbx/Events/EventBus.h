#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/Refs.h"
#include <atomic>
#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace Tbx
{
    /// <summary>
    /// Represents a function that consumes an <see cref="Event"/> instance.
    /// </summary>
    using EventCallback = std::function<void(Event&)>;

    using EventHash = uint64;

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
        EventBus(Ref<EventBus> parent = {});
        ~EventBus();

        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;
        EventBus(EventBus&&) noexcept = default;
        EventBus& operator=(EventBus&&) noexcept = default;

        /// <summary>
        /// Processes all queued events, dispatching each to the relevant subscribers.
        /// </summary>
        void Flush();

        /// <summary>
        /// Retrieves the callbacks registered for the supplied event key across this bus and its decorators.
        /// </summary>
        std::unordered_map<Uid, EventCallback> GetCallbacks(EventHash eventKey) const;

        /// <summary>
        /// Adds a subscription callback for the supplied event key.
        /// </summary>
        Uid AddSubscription(EventHash eventKey, EventCallback callback);

        /// <summary>
        /// Removes the subscription associated with the provided token.
        /// </summary>
        void RemoveSubscription(const Uid& token);

        /// <summary>
        /// Queues an event for dispatch during the next <see cref="Flush"/>.
        /// </summary>
        void QueueEvent(ExclusiveRef<Event> event);

        /// <summary>
        /// Returns the number of pending events waiting to be flushed.
        /// </summary>
        uint32 PendingEventCount() const;

    public:
        /// <summary>
        /// Retrieves the shared global event bus instance.
        /// </summary>
        static Ref<EventBus> Global;

    private:
        static Ref<EventBus> CreateGlobal();
        void CollectCallbacks(EventHash eventKey, std::unordered_map<Uid, EventCallback>& callbacks) const;
        void CollectCallbacksLocked(EventHash eventKey, std::unordered_map<Uid, EventCallback>& callbacks) const;
        void AttachToParent(EventBus* parent);
        void DetachFromParent();
        void DetachChildren(EventBus* adoptiveParent);
        void RegisterDecorator(const EventBus* decorator) const;
        void UnregisterDecorator(const EventBus* decorator) const;

    private:
        static bool _creatingGlobal;
        const EventBus* _parent = nullptr;
        mutable std::vector<const EventBus*> _decorators = {};
        std::unordered_map<EventHash, std::unordered_map<Uid, EventCallback>> _subscriptions = {};
        std::unordered_map<Uid, EventHash> _subscriptionIndex = {};
        std::queue<ExclusiveRef<Event>> _eventQueue = {};
        mutable std::mutex _mutex;
    };
}

