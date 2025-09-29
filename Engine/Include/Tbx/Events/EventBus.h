#pragma once
#include "Tbx/Callbacks/CallbackFunction.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/Hashing.h"
#include "Tbx/Memory/Refs.h"
#include <unordered_map>
#include <queue>
#include <functional>
#include <atomic>
#include <cstdarg>
#include <mutex>
#include <type_traits>
#include <typeindex>

namespace Tbx
{
    template <class TEvent>
    using EventHandlerFunction = void(*)(TEvent&);

    template <typename T, class TEvent>
    using ClassEventHandlerFunction = void(T::*)(TEvent&);

    /// <summary>
    /// A callback function that takes an event as a parameter.
    /// </summary>
    using EventCallback = CallbackFunction<Event>;

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

	// TODO: Use a subscription token system so subscription can be managed more easily via the lifetime of the token.
    class TBX_EXPORT EventBus
    {
    public:
        EventBus() = default;
        ~EventBus() = default;

        /// Subscribe free function (non-const)
        template <class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        void Subscribe(EventHandlerFunction<TEvent> callback)
        {
            auto eventKey = GetEventHash<TEvent>();
            auto callbackKey = GetCallbackHash(callback);
            AddSubscriber(eventKey, callbackKey, [callback](Event& e)
            {
                callback(static_cast<TEvent&>(e));
            });
        }

        /// Unsubscribe free function (non-const)
        template <class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        void Unsubscribe(EventHandlerFunction<TEvent> callback)
        {
            auto eventKey = GetEventHash<TEvent>();
            auto callbackKey = GetCallbackHash(callback);
            RemoveSubscriber(eventKey, callbackKey, typeid(TEvent));
        }

        /// Subscribe member function (non-const)
        template <typename TSubscriber, class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        void Subscribe(TSubscriber* instance, ClassEventHandlerFunction<TSubscriber, TEvent> callback)
        {
            auto eventKey = GetEventHash<TEvent>();
            auto callbackKey = GetCallbackHash(instance, callback);
            AddSubscriber(eventKey, callbackKey, [instance, callback](Event& e)
            {
                (instance->*callback)(static_cast<TEvent&>(e));
            });
        }

        /// Unsubscribe member function (non-const)
        template <typename TSubscriber, class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        void Unsubscribe(TSubscriber* instance, ClassEventHandlerFunction<TSubscriber, TEvent> callback)
        {
            auto eventKey = GetEventHash<TEvent>();
            auto callbackKey = GetCallbackHash(instance, callback);
            RemoveSubscriber(eventKey, callbackKey, typeid(TEvent));
        }

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
        void AddSubscriber(uint64 eventKey, uint64 callbackKey, std::function<void(Event&)> callable);
        void RemoveSubscriber(uint64 eventKey, uint64 callbackKey, const std::type_info& eventType);
        void SendEvent(Event& event);
        ExclusiveRef<Event> PopNextEventInQueue();
        uint64 GetEventHash(const Event& event) const;

        template <class TEvent>
        uint64 GetEventHash() const
        {
            const auto& eventInfo = typeid(TEvent);
            const auto hash = eventInfo.hash_code();
            return static_cast<uint64>(hash);
        }

        template <class TEvent>
        uint64 GetCallbackHash(EventHandlerFunction<TEvent> callback) const
        {
            const auto& callbackInfo = typeid(EventHandlerFunction<TEvent>);
            const auto typeHash = callbackInfo.hash_code();
            const auto callbackHash = reinterpret_cast<std::uintptr_t>(callback);
            return Memory::CombineHashes(typeHash, callbackHash);
        }

        template <class TEvent, typename TSubscriber>
        Tbx::uint64 GetCallbackHash(TSubscriber* instance, ClassEventHandlerFunction<TSubscriber, TEvent> callback) const
        {
            auto instanceHash = reinterpret_cast<std::uintptr_t>(instance);
            auto callbackHash = std::hash<void const*>{}(*reinterpret_cast<void const**>(&callback));
            auto typeHash = std::type_index(typeid(*instance)).hash_code();
            return Memory::CombineHashes(Memory::CombineHashes(typeHash, instanceHash), callbackHash);
        }

    private:
        mutable std::mutex _mutex = {};
        std::unordered_map<uint64, std::unordered_map<uint64, EventCallback>> _subscribers = {};
        std::queue<ExclusiveRef<Event>> _eventQueue = {};
    };
}
