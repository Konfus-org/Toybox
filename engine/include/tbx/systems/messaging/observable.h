#pragma once
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/messaging/message.h"
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
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
            TOwner& owner_ref,
            const TChanged& prev,
            const TChanged& curr);

        Observable<TOwner, TChanged> TOwner::* member = nullptr;
        std::reference_wrapper<TOwner> owner;
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
        template <typename, typename>
        friend class Observable;

        Observable(TOwner& owner_ref, Observable<TOwner, TProp> TOwner::* member_ptr, TProp val);

        Observable(
            std::weak_ptr<IMessageDispatcher> dispatcher,
            TOwner& owner_ref,
            Observable<TOwner, TProp> TOwner::* member_ptr,
            TProp val);

        template <typename TParentOwner>
        Observable(
            std::weak_ptr<IMessageDispatcher> dispatcher,
            Observable<TParentOwner, TOwner>& parent_property,
            TOwner& owner_ref,
            Observable<TOwner, TProp> TOwner::* member_ptr,
            TProp val);

        template <typename... TArgs>
        Observable(
            std::weak_ptr<IMessageDispatcher> dispatcher,
            TOwner& owner_ref,
            Observable<TOwner, TProp> TOwner::* member_ptr,
            std::in_place_t,
            TArgs&&... args);

        operator TProp&();
        operator const TProp&() const;

        TProp* operator->();
        const TProp* operator->() const;

        Observable& operator=(const TProp& v);
        Observable& operator=(TProp&& v);

        TOwner& get_parent();
        const TOwner& get_parent() const;

      private:
        std::weak_ptr<IMessageDispatcher> _dispatcher = {};
        std::function<void(const TProp&)> _notify_parent_property = {};
        Observable<TOwner, TProp> TOwner::* _member = nullptr;
        std::reference_wrapper<TOwner> owner;

      public:
        TProp value;

      private:
        template <typename TValue>
        void set_impl(TValue&& v);

        void notify(const TProp& previous, const TProp& current) const;
    };

    /// @brief
    /// Purpose: Extracts the owner and property types from an observable member pointer.
    /// @details
    /// Ownership: Type-only helper; no runtime ownership or storage.
    /// Thread Safety: Not applicable; compile-time only.
    template <auto TMember>
    struct ObservableMemberTraits;

    /// @brief
    /// Purpose: Provides typed information for observable member pointers.
    /// @details
    /// Ownership: Type-only helper; no runtime ownership or storage.
    /// Thread Safety: Not applicable; compile-time only.
    template <typename TOwner, typename TProp, Observable<TOwner, TProp> TOwner::* TMember>
    struct ObservableMemberTraits<TMember>
    {
        using Owner = TOwner;
        using Property = TProp;
        static constexpr Observable<TOwner, TProp> TOwner::* member = TMember;
    };

    /// @brief
    /// Purpose: Attempts to retrieve a property changed event for a specific observable member.
    /// @details
    /// Ownership: Non-owning; the output reference borrows from the input message.
    /// Thread Safety: Matches the caller's context. No synchronization is applied.
    template <auto TMember>
    std::optional<std::reference_wrapper<PropertyChangedEvent<typename ObservableMemberTraits<TMember>::Owner, typename ObservableMemberTraits<TMember>::Property>>> handle_property_changed(
        Message& msg);

    /// @brief
    /// Purpose: Attempts to retrieve a property changed event for a specific observable member.
    /// @details
    /// Ownership: Non-owning; the output reference borrows from the input message.
    /// Thread Safety: Matches the caller's context. No synchronization is applied.
    template <auto TMember>
    std::optional<std::reference_wrapper<const PropertyChangedEvent<typename ObservableMemberTraits<TMember>::Owner, typename ObservableMemberTraits<TMember>::Property>>> handle_property_changed(
        const Message& msg);

}

#include "tbx/systems/messaging/observable.inl"
