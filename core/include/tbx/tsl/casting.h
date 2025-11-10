#pragma once
#include "tbx/tsl/smart_pointers.h"
#include "tbx/tsl/variant.h"

namespace tbx
{
    template <typename TTo, typename TFrom>
    bool is(const TFrom* ptr)
    {
        return dynamic_cast<const TTo*>(ptr) != nullptr;
    }

    template <typename TTo, typename TFrom>
    bool is(const Ref<TFrom>& ref)
    {
        return is<TTo>(ref.get());
    }

    template <typename TTo>
    bool is(const Variant& value)
    {
        return value.has_value() && value.is<TTo>();
    }

    template <typename TTo, typename TFrom>
    TTo* as(TFrom* ptr)
    {
        return dynamic_cast<TTo*>(ptr);
    }

    template <typename TTo, typename TFrom>
    const TTo* as(const TFrom* ptr)
    {
        return dynamic_cast<const TTo*>(ptr);
    }

    template <typename TTo, typename TFrom>
    TTo* as(TFrom& ptr)
    {
        return as<TTo>(&ptr);
    }

    template <typename TTo, typename TFrom>
    const TTo* as(const TFrom& ptr)
    {
        return as<TTo>(&ptr);
    }

    template <typename TTo>
    TTo* as(Variant& value)
    {
        if (is<TTo>(value))
        {
            return &value.get_value<TTo>();
        }
        return nullptr;
    }

    template <typename TTo>
    TTo& as(Variant& value)
    {
        return value.get_value<TTo>();
    }

    template <typename TTo>
    const TTo& as(const Variant& value)
    {
        return value.get_value<TTo>();
    }

    template <typename TTo, typename TFrom>
    Ref<TTo> as(Ref<TFrom>& ref)
    {
        if (auto* derived = as<TTo>(ref.get()))
        {
            return Ref<TTo>(ref, derived);
        }
        return {};
    }

    template <typename TTo, typename TFrom>
    bool try_as(TFrom* ptr, TTo*& out)
    {
        out = as<TTo>(ptr);
        return out != nullptr;
    }

    template <typename TTo, typename TFrom>
    bool try_as(const TFrom* ptr, TTo*& out)
    {
        out = const_cast<TTo*>(as<TTo>(ptr));
        return out != nullptr;
    }

    template <typename TTo, typename TFrom>
    bool try_as(TFrom& from, TTo*& out)
    {
        return try_as<TTo>(&from, out);
    }

    template <typename TTo, typename TFrom>
    bool try_as(const TFrom& from, TTo*& out)
    {
        return try_as<TTo>(&from, out);
    }

    template <typename TTo, typename TFrom>
    bool try_as(const Ref<TFrom>& ref, Ref<TTo>& out)
    {
        if (auto* derived = as<TTo>(ref.get()))
        {
            out = Ref<TTo>(ref, derived);
            return true;
        }
        return false;
    }

    template <typename TTo, typename TFrom>
    bool try_as(Ref<TFrom>& ref, Ref<TTo>& out)
    {
        if (auto* derived = as<TTo>(ref.get()))
        {
            out = Ref<TTo>(ref, derived);
            return true;
        }
        return false;
    }

    template <typename TTo>
    bool try_as(Variant& value, TTo*& out)
    {
        if (is<TTo>(value))
        {
            out = &value.get_value<TTo>();
            return true;
        }

        out = nullptr;
        return false;
    }
}
