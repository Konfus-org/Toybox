#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Callbacks/CallbackFunction.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Memory/Hashing.h"
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>
#include <queue>
#include <functional>
#include <type_traits>

namespace Tbx
{
    template <class TEvent>
    using EventHandlerFunction = void(*)(TEvent&);

    template <class TEvent>
    using ConstEventHandlerFunction = void(*)(const TEvent&);

    template <typename T, class TEvent>
    using ClassEventHandlerFunction = void(T::*)(TEvent&);

    template <class TEvent, typename T>
    using ClassConstEventHandlerFunction = void(T::*)(const TEvent&);

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

        static std::atomic_int _suppressCount;
    };

    /// <summary>
    /// A class that manages event subscriptions and sends events to subscribers.
    /// </summary>
    class TBX_EXPORT EventBus
    {
    public:
        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// </summary>
        template <class TEvent>
        void Subscribe(EventHandlerFunction<TEvent> callback)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            auto eventKey = GetEventHash<TEvent>();
            auto callbackKey = GetCallbackHash(callback);

            std::lock_guard<std::mutex> lock(_mutex);
            if (_subscribers.contains(eventKey) == false)
            {
                _subscribers[eventKey] = {};
            }

            _subscribers[eventKey][callbackKey] = [callback](Event& event)
            {
                callback(static_cast<TEvent&>(event));
            };
        }

        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// </summary>
        template <class TEvent>
        void Subscribe(ConstEventHandlerFunction<TEvent> callback)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            auto eventKey = GetEventHash<TEvent>();
            auto callbackKey = GetCallbackHash(callback);

            std::lock_guard<std::mutex> lock(_mutex);
            if (_subscribers.contains(eventKey) == false)
            {
                _subscribers[eventKey] = {};
            }

            _subscribers[eventKey][callbackKey] = [callback](Event& event)
            {
                callback(static_cast<const TEvent&>(event));
            };
        }

        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// </summary>
        template <typename TSubscriber, class TEvent>
        void Subscribe(TSubscriber* instance, ClassEventHandlerFunction<TSubscriber, TEvent> callback)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            auto eventKey = GetEventHash<TEvent>();
            auto callbackKey = GetCallbackHash(instance, callback);

            std::lock_guard<std::mutex> lock(_mutex);
            if (_subscribers.contains(eventKey) == false)
            {
                _subscribers[eventKey] = {};
            }

            _subscribers[eventKey][callbackKey] = [instance, callback](Event& event)
            {
                (instance->*callback)(static_cast<TEvent&>(event));
            };
        }

        /// <summary>
        /// Sets a method to be called when an event is fired.
        /// </summary>
        template <typename TSubscriber, class TEvent>
        void Subscribe(TSubscriber* instance, ClassConstEventHandlerFunction<TSubscriber, TEvent> callback)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            auto eventKey = GetEventHash<TEvent>();
            auto callbackKey = GetCallbackHash(instance, callback);

            std::lock_guard<std::mutex> lock(_mutex);
            if (_subscribers.contains(eventKey) == false)
            {
                _subscribers[eventKey] = {};
            }

            _subscribers[eventKey][callbackKey] = [instance, callback](Event& event)
            {
                (instance->*callback)(static_cast<const TEvent&>(event));
            };
        }

        /// <summary>
        /// Removes the method associated with the given function from the list of callbacks for an event.
        /// </summary>
        template <class TEvent>
        void Unsubscribe(EventHandlerFunction<TEvent> callback)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            auto eventKey = GetEventHash<TEvent>();

            std::lock_guard<std::mutex> lock(_mutex);
            if (_subscribers.contains(eventKey) == false)
            {
                return;
            }

            auto callbackKey = GetCallbackHash(callback);
            auto& callbacks = _subscribers[eventKey];
            if (callbacks.erase(callbackKey) == 0)
            {
                const auto& eventInfo = typeid(TEvent);
                TBX_ASSERT(false, "Failed to unsubscribe from event. Callback not found!", eventInfo.name());
            }

            if (callbacks.empty())
            {
                _subscribers.erase(eventKey);
            }
        }

        template <class TEvent>
        void Unsubscribe(ConstEventHandlerFunction<TEvent> callback)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            auto eventKey = GetEventHash<TEvent>();

            std::lock_guard<std::mutex> lock(_mutex);
            if (_subscribers.contains(eventKey) == false)
            {
                return;
            }

            auto callbackKey = GetCallbackHash(callback);
            auto& callbacks = _subscribers[eventKey];
            if (callbacks.erase(callbackKey) == 0)
            {
                const auto& eventInfo = typeid(TEvent);
                TBX_ASSERT(false, "Failed to unsubscribe from event. Callback not found!", eventInfo.name());
            }

            if (callbacks.empty())
            {
                _subscribers.erase(eventKey);
            }
        }

        template <typename TSubscriber, class TEvent>
        void Unsubscribe(TSubscriber* instance, ClassEventHandlerFunction<TSubscriber, TEvent> callback)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            auto eventKey = GetEventHash<TEvent>();

            std::lock_guard<std::mutex> lock(_mutex);
            if (_subscribers.contains(eventKey) == false)
            {
                return;
            }

            auto callbackKey = GetCallbackHash(instance, callback);
            auto& callbacks = _subscribers[eventKey];
            if (callbacks.erase(callbackKey) == 0)
            {
                const auto& eventInfo = typeid(TEvent);
                TBX_ASSERT(false, "Failed to unsubscribe from event {}. Callback not found!", eventInfo.name());
            }

            if (callbacks.empty())
            {
                _subscribers.erase(eventKey);
            }
        }

        template <typename TSubscriber, class TEvent>
        void Unsubscribe(TSubscriber* instance, ClassConstEventHandlerFunction<TSubscriber, TEvent> callback)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            auto eventKey = GetEventHash<TEvent>();

            std::lock_guard<std::mutex> lock(_mutex);
            if (_subscribers.contains(eventKey) == false)
            {
                return;
            }

            auto callbackKey = GetCallbackHash(instance, callback);
            auto& callbacks = _subscribers[eventKey];
            if (callbacks.erase(callbackKey) == 0)
            {
                const auto& eventInfo = typeid(TEvent);
                TBX_ASSERT(false, "Failed to unsubscribe from event {}. Callback not found!", eventInfo.name());
            }

            if (callbacks.empty())
            {
                _subscribers.erase(eventKey);
            }
        }

        /// <summary>
        /// Immidiately processes an event and relays it to all subscribers.
        /// Returns true if the event was marked as handled, false otherwise.
        /// </summary>
        template <class TEvent>
        bool Send(TEvent&& event)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            TBX_TRACE_VERBOSE("Sending the event \"{}\"", event.ToString());

            const auto hashCode = GetEventHash(event);
            std::unordered_map<Tbx::uint64, EventCallback> callbacks = {};
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto subscriberIt = _subscribers.find(hashCode);
                if (subscriberIt == _subscribers.end())
                {
                    return false;
                }

                callbacks = subscriberIt->second;
            }

            for (auto& callback : callbacks)
            {
                if (EventSuppressor::IsSuppressing())
                {
                    TBX_TRACE("The event \"{}\" is suppressed", event.ToString());
                    return false;
                }
                callback.second(event);
            }

            return event.IsHandled;
        }

        /// <summary>
        /// Posts an event that will be processed and relayed to subscribers the next update.
        /// </summary>
        template <class TEvent>
        void Post(TEvent&& event)
        {
            static_assert(std::is_base_of_v<Event, std::decay_t<TEvent>>, "TEvent must derive from Event");

            TBX_TRACE_VERBOSE("Posting the event \"{}\"", event.ToString());

            std::lock_guard<std::mutex> lock(_mutex);
            using U = std::decay_t<TEvent>;
            _eventQueue.emplace(std::make_unique<U>(std::forward<TEvent>(event)));
        }

        /// <summary>
        /// Processes all queued events.
        /// </summary>
        void ProcessQueue();

    private:
        template <class TEvent>
        Tbx::uint64 GetEventHash() const
        {
            const auto& eventInfo = typeid(TEvent);
            const auto hash = eventInfo.hash_code();
            return static_cast<Tbx::uint64>(hash);
        }

        template <class TEvent>
        Tbx::uint64 GetCallbackHash(EventHandlerFunction<TEvent> callback) const
        {
            const auto& callbackInfo = typeid(EventHandlerFunction<TEvent>);
            const auto typeHash = static_cast<Tbx::uint64>(callbackInfo.hash_code());
            const auto callbackHash = static_cast<Tbx::uint64>(reinterpret_cast<std::uintptr_t>(callback));
            return Memory::CombineHashes(typeHash, callbackHash);
        }

        template <class TEvent>
        Tbx::uint64 GetCallbackHash(ConstEventHandlerFunction<TEvent> callback) const
        {
            const auto& callbackInfo = typeid(ConstEventHandlerFunction<TEvent>);
            const auto typeHash = static_cast<Tbx::uint64>(callbackInfo.hash_code());
            const auto callbackHash = static_cast<Tbx::uint64>(reinterpret_cast<std::uintptr_t>(callback));
            return Memory::CombineHashes(typeHash, callbackHash);
        }

        template <class TEvent, typename TSubscriber>
        Tbx::uint64 GetCallbackHash(TSubscriber* instance, ClassEventHandlerFunction<TSubscriber, TEvent> callback) const
        {
            const auto& callbackInfo = typeid(ClassEventHandlerFunction<TSubscriber, TEvent>);
            const auto typeHash = static_cast<Tbx::uint64>(callbackInfo.hash_code());
            const auto instanceHash = static_cast<Tbx::uint64>(std::hash<TSubscriber*>{}(instance));
            const auto callbackHash = HashMemberFunctionPointer(callback);
            return Memory::CombineHashes(Memory::CombineHashes(typeHash, instanceHash), callbackHash);
        }

        template <class TEvent, typename TSubscriber>
        Tbx::uint64 GetCallbackHash(TSubscriber* instance, ClassConstEventHandlerFunction<TSubscriber, TEvent> callback) const
        {
            const auto& callbackInfo = typeid(ClassConstEventHandlerFunction<TSubscriber, TEvent>);
            const auto typeHash = static_cast<Tbx::uint64>(callbackInfo.hash_code());
            const auto instanceHash = static_cast<Tbx::uint64>(std::hash<TSubscriber*>{}(instance));
            const auto callbackHash = HashMemberFunctionPointer(callback);
            return Memory::CombineHashes(Memory::CombineHashes(typeHash, instanceHash), callbackHash);
        }

        template <typename TMemberFunction>
        Tbx::uint64 HashMemberFunctionPointer(TMemberFunction callback) const
        {
            static_assert(std::is_member_function_pointer_v<TMemberFunction>, "TMemberFunction must be a pointer to member function");

            constexpr Tbx::uint64 FNV_OFFSET_BASIS = 1469598103934665603ULL;
            constexpr Tbx::uint64 FNV_PRIME = 1099511628211ULL;

            const auto* bytes = reinterpret_cast<const unsigned char*>(&callback);
            Tbx::uint64 hash = FNV_OFFSET_BASIS;
            for (size_t index = 0; index < sizeof(TMemberFunction); ++index)
            {
                hash ^= static_cast<Tbx::uint64>(bytes[index]);
                hash *= FNV_PRIME;
            }

            return hash;
        }

        Tbx::uint64 GetEventHash(const Event& event) const;
        std::unique_ptr<Event> PopNextEventInQueue();

        std::unordered_map<Tbx::uint64, std::unordered_map<Tbx::uint64, EventCallback>> _subscribers = {};
        std::queue<ExclusiveRef<Event>> _eventQueue = {};
        std::mutex _mutex = {};
    };
}
