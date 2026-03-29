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
    struct PropertyChangedEvent;

    // A property that is observable
    // When changed it will send PropertyChanged events
    // Ownership: value type; callers own any copies created from this class.
    // Thread-safety: not inherently thread-safe; synchronize access when sharing instances.
    template <typename TOwner, typename TProp>
    class Observable;

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
    struct ObservableMemberTraits<TMember>;

    /// @brief
    /// Purpose: Attempts to retrieve a property changed event for a specific observable member.
    /// @details
    /// Ownership: Non-owning; the output pointer borrows from the input message.
    /// Thread Safety: Matches the caller's context. No synchronization is applied.
    template <auto TMember>
    PropertyChangedEvent<typename ObservableMemberTraits<TMember>::Owner, typename ObservableMemberTraits<TMember>::Property>* handle_property_changed(
        Message& msg);

    /// @brief
    /// Purpose: Attempts to retrieve a property changed event for a specific observable member.
    /// @details
    /// Ownership: Non-owning; the output pointer borrows from the input message.
    /// Thread Safety: Matches the caller's context. No synchronization is applied.
    template <auto TMember>
    const PropertyChangedEvent<typename ObservableMemberTraits<TMember>::Owner, typename ObservableMemberTraits<TMember>::Property>* handle_property_changed(
        const Message& msg);
}

#include "tbx/messages/observable.inl"
