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
    /// A callback function that takes an event as a parameter.
    /// </summary>
    using EventCallback = std::function<void(Event&)>;

    /// <summary>
    /// ALL events on ALL event busses will be suppressed during the lifetime of this class.
    /// Use with care.
    /// </summary>
    class TBX_EXPORT EventSuppressor
    {
    public:
        EventSuppressor();
        ~EventSuppressor();

        static bool IsSuppressing();

    private:
        static void Suppress();
        static void Unsuppress();

    private:
        static std::atomic_int _suppressCount;
    };

    class TBX_EXPORT EventBus
    {
    public:
        EventBus() = default;
        ~EventBus() = default;

        using EventHash = uint64;

        /// Subscribes an event callback and returns a token that can be used to unsubscribe.
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

        /// Removes a subscription associated with the provided token.
        void Unsubscribe(const Uid& token);

        /// Immediately processes an event and relays it to all subscribers.
        /// Returns true if the event was marked as handled, false otherwise.
        template <class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        bool Send(TEvent&& event)
        {
            TBX_TRACE_VERBOSE("Event Bus: Sending the event \"{}\"", event.ToString());
            SendEvent(event);
            return event.IsHandled;
        }

        /// Posts an event that will be processed and relayed to subscribers the next update.
        template <class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        void Post(TEvent event)
        {
            TBX_TRACE_VERBOSE("Event Bus: Posting the event \"{}\"", event.ToString());
            std::scoped_lock lock(_mutex);
            _eventQueue.emplace(std::make_unique<std::decay_t<TEvent>>(std::move(event)));
        }

        /// Processes all queued events.
        void ProcessQueue();

    private:
        void AddSubscriber(EventHash eventKey, const Uid& token, EventCallback callable);
        void RemoveSubscriber(const Uid& token);
        void SendEvent(Event& event);
        ExclusiveRef<Event> PopNextEventInQueue();
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
