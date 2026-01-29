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

            _dispatcher
                ->send<PropertyChangedEvent<TOwner, TProp>>(_member, owner, previous, current);
        }

      private:
        IMessageDispatcher* _dispatcher = nullptr;
        Observable<TOwner, TProp> TOwner::* _member = nullptr;
    };

    /// <summary>
    /// Purpose: Extracts the owner and property types from an observable member pointer.
    /// </summary>
    /// <remarks>
    /// Ownership: Type-only helper; no runtime ownership or storage.
    /// Thread Safety: Not applicable; compile-time only.
    /// </remarks>
    template <auto TMember>
    struct ObservableMemberTraits;

    /// <summary>
    /// Purpose: Provides typed information for observable member pointers.
    /// </summary>
    /// <remarks>
    /// Ownership: Type-only helper; no runtime ownership or storage.
    /// Thread Safety: Not applicable; compile-time only.
    /// </remarks>
    template <typename TOwner, typename TProp, Observable<TOwner, TProp> TOwner::* TMember>
    struct ObservableMemberTraits<TMember>
    {
        using Owner = TOwner;
        using Property = TProp;
        static constexpr Observable<TOwner, TProp> TOwner::* member = TMember;
    };

    /// <summary>
    /// Purpose: Attempts to retrieve a property changed event for a specific observable member.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning; the output pointer borrows from the input message.
    /// Thread Safety: Matches the caller's context. No synchronization is applied.
    /// </remarks>
    template <auto TMember>
    PropertyChangedEvent<
        typename ObservableMemberTraits<TMember>::Owner,
        typename ObservableMemberTraits<TMember>::Property>*
        handle_property_changed(Message& msg)
    {
        using Traits = ObservableMemberTraits<TMember>;
        auto* typed = handle_message<
            PropertyChangedEvent<typename Traits::Owner, typename Traits::Property>>(msg);
        if (!typed || typed->member != Traits::member)
        {
            return nullptr;
        }

        return typed;
    }

    /// <summary>
    /// Purpose: Attempts to retrieve a property changed event for a specific observable member.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning; the output pointer borrows from the input message.
    /// Thread Safety: Matches the caller's context. No synchronization is applied.
    /// </remarks>
    template <auto TMember>
    const PropertyChangedEvent<
        typename ObservableMemberTraits<TMember>::Owner,
        typename ObservableMemberTraits<TMember>::Property>*
        handle_property_changed(const Message& msg)
    {
        using Traits = ObservableMemberTraits<TMember>;
        const auto* typed = handle_message<
            PropertyChangedEvent<typename Traits::Owner, typename Traits::Property>>(msg);
        if (!typed || typed->member != Traits::member)
        {
            return nullptr;
        }

        return typed;
    }
}
