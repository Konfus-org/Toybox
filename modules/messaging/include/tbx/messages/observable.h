#pragma once
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/message.h"
#include <concepts>
#include <functional>
#include <string>
#include <utility>

namespace tbx
{
    template <typename TOwner, typename TProp>
    class Observable;

    // Event sent when an observable property changes
    // Ownership: typically stack-allocated and passed by reference to handlers.
    // Thread-safety: safe for concurrent read-only access; synchronize if mutable access is needed.
    template <typename TOwner, typename TChanged>
    struct PropertyChangedEvent : Event
    {
        PropertyChangedEvent(
            Observable<TOwner, TChanged> TOwner::* member_ptr,
            TOwner* owner_ptr,
            const TChanged& prev,
            const TChanged& curr)
            : member(member_ptr)
            , owner(owner_ptr)
            , previous(prev)
            , current(curr)
        {
        }

        Observable<TOwner, TChanged> TOwner::* member = nullptr;
        TOwner* owner = nullptr;
        TChanged previous;
        TChanged current;
    };

    // A property that is observable
    // When changed it will send PropertyChanged events
    // Ownership: value type; callers own any copies created from this class.
    // Thread-safety: not inherently thread-safe; synchronize access when sharing instances.
    template <typename TOwner, typename TProp>
    class Observable
    {
      public:
        Observable(
            IMessageDispatcher* dispatch,
            TOwner* owner,
            Observable<TOwner, TProp> TOwner::* member_ptr,
            TProp val)
            : _dispatcher(dispatch)
            , _member(member_ptr)
            , owner(owner)
            , value(val)
        {
            notify(value, value);
        }

        operator TProp&()
        {
            return value;
        }

        operator const TProp&() const
        {
            return value;
        }

        Observable& operator=(const TProp& v)
        {
            set_impl(v);
            return *this;
        }

        Observable& operator=(TProp&& v)
        {
            set_impl(std::move(v));
            return *this;
        }

      public:
        TProp value;
        TOwner* owner;

      private:
        template <typename TValue>
        void set_impl(TValue&& v)
        {
            if constexpr (requires(const TProp& a, const TValue& b) {
                              { a == b } -> std::convertible_to<bool>;
                          })
            {
                if (value == v)
                    return;
            }

            TProp prev = value;
            value = std::forward<TValue>(v);

            notify(prev, value);
        }

        void notify(const TProp& previous, const TProp& current) const
        {
            if (!_dispatcher)
                return;

            auto event = PropertyChangedEvent<TOwner, TProp>(_member, owner, previous, current);
            _dispatcher->send(event);
        }

      private:
        IMessageDispatcher* _dispatcher = nullptr;
        Observable<TOwner, TProp> TOwner::* _member = nullptr;
    };

    // Helper to handle property changed events
    template <typename TOwner, typename TProp, typename Handler>
    bool on_property_changed(
        Message& msg,
        Observable<TOwner, TProp> TOwner::* property,
        Handler&& handler)
    {
        if (auto* typed = dynamic_cast<PropertyChangedEvent<TOwner, TProp>*>(&msg))
        {
            if (typed->member == property)
            {
                handler(*typed);
                return true;
            }
        }
        return false;
    }

    // Helper to handle property changed events
    template <typename TOwner, typename TProp, typename Handler>
    bool on_property_changed(
        const Message& msg,
        Observable<TOwner, TProp> TOwner::* property,
        Handler&& handler)
    {
        if (const auto* typed = dynamic_cast<const PropertyChangedEvent<TOwner, TProp>*>(&msg))
        {
            if (typed->member == property)
            {
                handler(*typed);
                return true;
            }
        }
        return false;
    }
}
