#pragma once
#include "tbx/std/smart_pointers.h"
#include "tbx/std/variant.h"

namespace tbx
{
    template <typename TTo, typename TFrom>
    bool is(const TFrom* ptr);

    template <typename TTo, typename TFrom>
    bool is(const Ref<TFrom>& ref);

    template <typename TTo>
    bool is(const Variant& value);

    template <typename TTo, typename TFrom>
    TTo* as(TFrom* ptr);

    template <typename TTo, typename TFrom>
    const TTo* as(const TFrom* ptr);

    template <typename TTo, typename TFrom>
    TTo* as(TFrom& ptr);

    template <typename TTo, typename TFrom>
    const TTo* as(const TFrom& ptr);

    template <typename TTo>
    TTo* as(Variant& value);

    template <typename TTo>
    TTo& as(Variant& value);

    template <typename TTo>
    const TTo& as(const Variant& value);

    template <typename TTo, typename TFrom>
    Ref<TTo> as(Ref<TFrom>& ref);

    template <typename TTo, typename TFrom>
    bool try_as(TFrom* ptr, TTo*& out);

    template <typename TTo, typename TFrom>
    bool try_as(const TFrom* ptr, TTo*& out);

    template <typename TTo, typename TFrom>
    bool try_as(TFrom& from, TTo*& out);

    template <typename TTo, typename TFrom>
    bool try_as(const TFrom& from, TTo*& out);

    template <typename TTo, typename TFrom>
    bool try_as(const Ref<TFrom>& ref, Ref<TTo>& out);

    template <typename TTo, typename TFrom>
    bool try_as(Ref<TFrom>& ref, Ref<TTo>& out);

    template <typename TTo>
    bool try_as(Variant& value, TTo*& out);
}

#include "../../../src/std/casting.inl"
