#pragma once

namespace tbx
{
    template <typename TValue>
    bool PayloadResult<TValue>::has_payload() const
    {
        return _payload.has_value();
    }

    template <typename TValue>
    const TValue& PayloadResult<TValue>::get_payload() const
    {
        return *_payload;
    }

    template <typename TValue>
    void PayloadResult<TValue>::set_payload(TValue value)
    {
        _payload = value;
    }

    template <typename TValue>
    void PayloadResult<TValue>::reset_payload()
    {
        _payload.reset();
    }
}
