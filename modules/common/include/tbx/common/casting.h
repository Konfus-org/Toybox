#pragma once
#include "tbx/common/smart_pointers.h"
#include <any>

namespace tbx
{
    template <typename TTo, typename TFrom>
    inline bool is(const TFrom* ptr)
    {
        return dynamic_cast<const TTo*>(ptr) != nullptr;
    }

    template <typename TTo, typename TFrom>
    inline bool is(const Ref<TFrom>& ref)
    {
        return is<TTo>(ref.get());
    }

    template <typename TTo, typename TFrom>
    inline TTo* as(TFrom* ptr)
    {
        return dynamic_cast<TTo*>(ptr);
    }

    template <typename TTo, typename TFrom>
    inline const TTo* as(const TFrom* ptr)
    {
        return dynamic_cast<const TTo*>(ptr);
    }

    template <typename TTo, typename TFrom>
    inline TTo* as(TFrom& ptr)
    {
        return as<TTo>(&ptr);
    }

    template <typename TTo, typename TFrom>
    inline const TTo* as(const TFrom& ptr)
    {
        return as<TTo>(&ptr);
    }

    template <typename TTo, typename TFrom>
    inline Ref<TTo> as(Ref<TFrom>& ref)
    {
        if (auto* derived = as<TTo>(ref.get()))
        {
            return Ref<TTo>(ref, derived);
        }
        return {};
    }

    template <typename TTo, typename TFrom>
    inline bool try_as(TFrom* ptr, TTo*& out)
    {
        out = as<TTo>(ptr);
        return out != nullptr;
    }

    template <typename TTo, typename TFrom>
    inline bool try_as(const TFrom* ptr, TTo*& out)
    {
        out = const_cast<TTo*>(as<TTo>(ptr));
        return out != nullptr;
    }

    template <typename TTo, typename TFrom>
    inline bool try_as(TFrom& from, TTo*& out)
    {
        return try_as<TTo>(&from, out);
    }

    template <typename TTo, typename TFrom>
    inline bool try_as(const TFrom& from, TTo*& out)
    {
        return try_as<TTo>(&from, out);
    }

    template <typename TTo, typename TFrom>
    inline bool try_as(const Ref<TFrom>& ref, Ref<TTo>& out)
    {
        if (auto* derived = as<TTo>(ref.get()))
        {
            out = Ref<TTo>(ref, derived);
            return true;
        }
        return false;
    }

    template <typename TTo, typename TFrom>
    inline bool try_as(Ref<TFrom>& ref, Ref<TTo>& out)
    {
        if (auto* derived = as<TTo>(ref.get()))
        {
            out = Ref<TTo>(ref, derived);
            return true;
        }
        return false;
    }

    template <typename TTo>
    inline bool is(const std::any& value)
    {
        return value.has_value() && value.type() == typeid(TTo);
    }

    template <typename TTo>
    inline TTo* as(std::any& value)
    {
        return std::any_cast<TTo>(&value);
    }

    template <typename TTo>
    inline TTo& as(std::any& value)
    {
        return std::any_cast<TTo&>(value);
    }

    template <typename TTo>
    inline const TTo& as(const std::any& value)
    {
        return std::any_cast<const TTo&>(value);
    }

    template <typename TTo>
    inline bool try_as(std::any& value, TTo*& out)
    {
        if (auto* converted = std::any_cast<TTo>(&value))
        {
            out = converted;
            return true;
        }

        out = nullptr;
        return false;
    }
}
