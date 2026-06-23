#pragma once

namespace tbx
{
    template <typename TOwner, typename TChanged>
    PropertyChangedEvent<TOwner, TChanged>::PropertyChangedEvent(
        Observable<TOwner, TChanged> TOwner::* member_ptr,
        TOwner& owner_ref,
        const TChanged& prev,
        const TChanged& curr)
        : member(member_ptr)
        , owner(owner_ref)
        , previous(prev)
        , current(curr)
    {
    }

    template <typename TOwner, typename TProp>
    Observable<TOwner, TProp>::Observable(
        TOwner& owner_ref, Observable<TOwner, TProp> TOwner::* member_ptr, TProp val)
        : Observable(std::weak_ptr<IMessageDispatcher>(), owner_ref, member_ptr, std::move(val))
    {
    }

    template <typename TOwner, typename TProp>
    Observable<TOwner, TProp>::Observable(
        std::weak_ptr<IMessageDispatcher> dispatcher,
        TOwner& owner_ref,
        Observable<TOwner, TProp> TOwner::* member_ptr,
        TProp val)
        : _dispatcher(std::move(dispatcher))
        , _notify_parent_property()
        , _member(member_ptr)
        , owner(owner_ref)
        , value(std::move(val))
    {
        notify(value, value);
    }

    template <typename TOwner, typename TProp>
    template <typename TParentOwner>
    Observable<TOwner, TProp>::Observable(
        std::weak_ptr<IMessageDispatcher> dispatcher,
        Observable<TParentOwner, TOwner>& parent_property,
        TOwner& owner_ref,
        Observable<TOwner, TProp> TOwner::* member_ptr,
        TProp val)
        : _dispatcher(std::move(dispatcher))
        , _notify_parent_property(
              [member_ptr, &parent_property](const TProp& previous_value)
              {
                  auto previous_parent_value = parent_property.value;
                  (previous_parent_value.*member_ptr).value = previous_value;
                  parent_property.notify(previous_parent_value, parent_property.value);
              })
        , _member(member_ptr)
        , owner(owner_ref)
        , value(std::move(val))
    {
    }

    template <typename TOwner, typename TProp>
    template <typename... TArgs>
    Observable<TOwner, TProp>::Observable(
        std::weak_ptr<IMessageDispatcher> dispatcher,
        TOwner& owner_ref,
        Observable<TOwner, TProp> TOwner::* member_ptr,
        std::in_place_t,
        TArgs&&... args)
        : _dispatcher(std::move(dispatcher))
        , _notify_parent_property()
        , _member(member_ptr)
        , owner(owner_ref)
        , value(_dispatcher, *this, std::forward<TArgs>(args)...)
    {
        notify(value, value);
    }

    template <typename TOwner, typename TProp>
    Observable<TOwner, TProp>::operator TProp&()
    {
        return value;
    }

    template <typename TOwner, typename TProp>
    Observable<TOwner, TProp>::operator const TProp&() const
    {
        return value;
    }

    template <typename TOwner, typename TProp>
    TProp* Observable<TOwner, TProp>::operator->()
    {
        return &value;
    }

    template <typename TOwner, typename TProp>
    const TProp* Observable<TOwner, TProp>::operator->() const
    {
        return &value;
    }

    template <typename TOwner, typename TProp>
    Observable<TOwner, TProp>& Observable<TOwner, TProp>::operator=(const TProp& v)
    {
        set_impl(v);
        return *this;
    }

    template <typename TOwner, typename TProp>
    Observable<TOwner, TProp>& Observable<TOwner, TProp>::operator=(TProp&& v)
    {
        set_impl(std::move(v));
        return *this;
    }

    template <typename TOwner, typename TProp>
    TOwner& Observable<TOwner, TProp>::get_parent()
    {
        return owner.get();
    }

    template <typename TOwner, typename TProp>
    const TOwner& Observable<TOwner, TProp>::get_parent() const
    {
        return owner.get();
    }

    template <typename TOwner, typename TProp>
    template <typename TValue>
    void Observable<TOwner, TProp>::set_impl(TValue&& v)
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

    template <typename TOwner, typename TProp>
    void Observable<TOwner, TProp>::notify(const TProp& previous, const TProp& current) const
    {
        auto dispatcher = _dispatcher.lock();
        if (!dispatcher)
            return;

        dispatcher->send<PropertyChangedEvent<TOwner, TProp>>(
            _member,
            owner.get(),
            previous,
            current);
        if (_notify_parent_property)
            _notify_parent_property(previous);
    }

    template <auto TMember>
    std::optional<std::reference_wrapper<PropertyChangedEvent<typename ObservableMemberTraits<TMember>::Owner, typename ObservableMemberTraits<TMember>::Property>>> handle_property_changed(
        Message& msg)
    {
        using Traits = ObservableMemberTraits<TMember>;
        auto typed =
            handle_message<PropertyChangedEvent<typename Traits::Owner, typename Traits::Property>>(
                msg);
        if (!typed.has_value() || typed->get().member != Traits::member)
        {
            return std::nullopt;
        }

        return typed;
    }

    template <auto TMember>
    std::optional<std::reference_wrapper<const PropertyChangedEvent<typename ObservableMemberTraits<TMember>::Owner, typename ObservableMemberTraits<TMember>::Property>>> handle_property_changed(
        const Message& msg)
    {
        using Traits = ObservableMemberTraits<TMember>;
        const auto typed =
            handle_message<PropertyChangedEvent<typename Traits::Owner, typename Traits::Property>>(
                msg);
        if (!typed.has_value() || typed->get().member != Traits::member)
        {
            return std::nullopt;
        }

        return typed;
    }

}
