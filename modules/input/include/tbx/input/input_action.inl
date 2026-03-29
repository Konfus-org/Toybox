#pragma once

namespace tbx
{
    template <typename TValue>
    bool InputAction::try_get_value_as(TValue& out_value) const
    {
        if (!std::holds_alternative<TValue>(_value))
            return false;

        out_value = std::get<TValue>(_value);
        return true;
    }

    template <typename TValue>
    const TValue* InputAction::try_get_value_as() const
    {
        if (!std::holds_alternative<TValue>(_value))
            return nullptr;

        return &std::get<TValue>(_value);
    }
}
