#pragma once

namespace tbx
{
    template <typename TTo, typename TFrom>
    bool is(const TFrom* ptr)
    {
        return dynamic_cast<const TTo*>(ptr) != nullptr;
    }

    template <typename TTo, typename TFrom>
    bool is(TFrom* ptr)
    {
        return dynamic_cast<const TTo*>(ptr) != nullptr;
    }

    // Preserve const-correctness when casting from const base pointers.
    template <typename TTo, typename TFrom>
    const TTo* as(const TFrom* ptr)
    {
        const TTo* derived = dynamic_cast<const TTo*>(ptr);
        return derived;
    }

    template <typename TTo, typename TFrom>
    TTo* as(TFrom* ptr)
    {
        TTo* derived = dynamic_cast<TTo*>(ptr);
        return derived;
    }

    template <typename TTo, typename TFrom>
    TTo* as(TFrom& ptr)
    {
        return as<TFrom>(&ptr);
    }

    template <typename TTo, typename TFrom>
    TTo* as(const TFrom& ptr)
    {
        return as<TFrom>(&ptr);
    }

    template <typename TTo, typename TFrom>
    bool try_as(TFrom* ptr, TTo* out)
    {
        if (!ptr)
        {
            return false;
        }

        TTo* derived = dynamic_cast<TTo*>(ptr);
        out = derived;

        return derived != nullptr;
    }

    template <typename TTo, typename TFrom>
    bool try_as(const TFrom* ptr, TTo* out)
    {
        if (!ptr)
        {
            return false;
        }

        const TTo* derived = dynamic_cast<const TTo*>(ptr);
        if (!derived)
        {
            return false;
        }

        out = const_cast<TTo*>(derived);
        return true;
    }

    template <typename TTo, typename TFrom>
    bool try_as(TFrom& from, TTo* out)
    {
        return try_as<TTo>(&from, out);
    }

    template <typename TTo, typename TFrom>
    bool try_as(const TFrom& from, TTo* out)
    {
        return try_as<TTo>(&from, out);
    }
}
