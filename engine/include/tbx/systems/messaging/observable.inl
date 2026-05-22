#pragma once

namespace tbx
{
    template <typename TOwner, typename TChanged>
    struct PropertyChangedEvent : Event
    {
        PropertyChangedEvent(
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

        Observable<TOwner, TChanged> TOwner::* member = nullptr;
        std::reference_wrapper<TOwner> owner;
        TChanged previous;
        TChanged current;
    };

    template <typename TOwner, typename TProp>
    class Observable
    {
      public:
        template <typename, typename>
        friend class Observable;

        Observable(TOwner& owner_ref, Observable<TOwner, TProp> TOwner::* member_ptr, TProp val)
            : Observable(nullptr, owner_ref, member_ptr, std::move(val))
        {
        }

        Observable(
            IMessageDispatcher& dispatch,
            TOwner& owner_ref,
            Observable<TOwner, TProp> TOwner::* member_ptr,
            TProp val)
            : Observable(&dispatch, owner_ref, member_ptr, std::move(val))
        {
        }

        Observable(
            IMessageDispatcher* dispatch,
            TOwner& owner_ref,
            Observable<TOwner, TProp> TOwner::* member_ptr,
            TProp val)
            : _dispatcher(dispatch)
            , _notify_parent_property()
            , _member(member_ptr)
            , owner(owner_ref)
            , value(std::move(val))
        {
            notify(value, value);
        }

        template <typename TParentOwner>
        Observable(
            IMessageDispatcher& dispatch,
            Observable<TParentOwner, TOwner>& parent_property,
            TOwner& owner_ref,
            Observable<TOwner, TProp> TOwner::* member_ptr,
            TProp val)
            : _dispatcher(&dispatch)
            , _notify_parent_property(
                  [member_ptr, &parent_property](const TProp& previous_value)
                  {
                      auto previous_parent_value = parent_property.value;
                      previous_parent_value.*member_ptr = previous_value;
                      parent_property.notify(previous_parent_value, parent_property.value);
                  })
            , _member(member_ptr)
            , owner(owner_ref)
            , value(std::move(val))
        {
        }

        template <typename... TArgs>
        Observable(
            IMessageDispatcher& dispatch,
            TOwner& owner_ref,
            Observable<TOwner, TProp> TOwner::* member_ptr,
            std::in_place_t,
            TArgs&&... args)
            : _dispatcher(&dispatch)
            , _notify_parent_property()
            , _member(member_ptr)
            , owner(owner_ref)
            , value(dispatch, *this, std::forward<TArgs>(args)...)
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

        TProp* operator->()
        {
            return &value;
        }

        const TProp* operator->() const
        {
            return &value;
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

        TOwner& get_parent()
        {
            return owner.get();
        }

        const TOwner& get_parent() const
        {
            return owner.get();
        }

      private:
        IMessageDispatcher* _dispatcher = nullptr;
        std::function<void(const TProp&)> _notify_parent_property = {};
        Observable<TOwner, TProp> TOwner::* _member = nullptr;
        std::reference_wrapper<TOwner> owner;

      public:
        TProp value;

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

            _dispatcher->send<PropertyChangedEvent<TOwner, TProp>>(
                _member,
                owner.get(),
                previous,
                current);
            if (_notify_parent_property)
                _notify_parent_property(previous);
        }
    };

    template <typename TOwner, typename TProp, Observable<TOwner, TProp> TOwner::* TMember>
    struct ObservableMemberTraits<TMember>
    {
        using Owner = TOwner;
        using Property = TProp;
        static constexpr Observable<TOwner, TProp> TOwner::* member = TMember;
    };

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
