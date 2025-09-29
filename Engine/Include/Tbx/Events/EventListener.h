#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Ids/Uid.h"
#include "Tbx/Memory/Refs.h"
#include <functional>
#include <mutex>
#include <type_traits>
#include <unordered_set>
#include <utility>

namespace Tbx
{
    template <class TEvent>
    using EventHandlerFunction = void(*)(TEvent&);

    template <typename T, class TEvent>
    using ClassEventHandlerFunction = void(T::*)(TEvent&);

    class TBX_EXPORT EventListener
    {
    public:
        EventListener() = default;
        explicit EventListener(const Ref<EventBus>& bus);
        explicit EventListener(const WeakRef<EventBus>& bus);
        ~EventListener();

        void Bind(const Ref<EventBus>& bus);
        void Bind(const WeakRef<EventBus>& bus);
        void Unbind();

        template <typename TSubscriber, class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        void Listen(TSubscriber* instance, ClassEventHandlerFunction<TSubscriber, TEvent> callback)
        {
            TBX_ASSERT(instance != nullptr, "EventListener: Cannot subscribe with a null instance.");
            Listen<TEvent>([instance, callback](TEvent& event)
            {
                std::invoke(callback, instance, event);
            });
        }

        template <class TEvent, typename TCallable>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>> &&
                 std::is_invocable_r_v<void, TCallable, TEvent&>
        void Listen(TCallable&& callable)
        {
            auto bus = LockBus();
            TBX_ASSERT(bus, "EventListener: Cannot listen without a bound event bus.");

            EventCallback callback = [fn = std::forward<TCallable>(callable)](Event& event)
            {
                std::invoke(fn, static_cast<TEvent&>(event));
            };

            const auto token = bus->Subscribe<TEvent>(std::move(callback));
            TrackToken(token);
        }

        template <class TEvent>
        requires std::is_base_of_v<Event, std::decay_t<TEvent>>
        void Listen(EventHandlerFunction<TEvent> callback)
        {
            TBX_ASSERT(callback != nullptr, "EventListener: Cannot subscribe a null callback.");
            Listen<TEvent>([callback](TEvent& event)
            {
                callback(event);
            });
        }

        void StopListening();

        bool IsBound() const;

    private:
        void TrackToken(const Uid& token);
        Ref<EventBus> LockBus() const;

    private:
        WeakRef<EventBus> _bus = {};
        std::unordered_set<Uid> _activeTokens = {};
        mutable std::mutex _mutex = {};
    };
}

