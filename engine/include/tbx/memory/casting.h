#pragma once

namespace tbx
{
    template <typename TTo, typename TFrom>
    bool is(const TFrom* ptr)
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
    bool try_as(TFrom* ptr, TTo* out)
    {
        TTo* derived = dynamic_cast<TTo*>(ptr);
        if (!derived)
        {
            return false;
        }
        out = derived;
        return true;
    }
}
