#pragma once

namespace tbx
{
    inline Variant::Variant() = default;

    template <typename TValue, typename>
    inline Variant::Variant(TValue&& value)
    {
        _storage = std::forward<TValue>(value);
    }

    inline Variant::Variant(const Variant&) = default;

    inline Variant::Variant(Variant&&) noexcept = default;

    inline Variant::~Variant() = default;

    inline bool Variant::has_value() const
    {
        return _storage.has_value();
    }

    inline bool Variant::is_empty() const
    {
        return !has_value();
    }

    inline void Variant::reset()
    {
        _storage.reset();
    }

    template <typename TValue>
    inline bool Variant::is() const
    {
        using TValueNoRef = std::remove_cv_t<std::remove_reference_t<TValue>>;
        return std::any_cast<TValueNoRef>(&_storage) != nullptr;
    }

    template <typename TValue>
    inline TValue& Variant::get_value()
    {
        return std::any_cast<TValue&>(_storage);
    }

    template <typename TValue>
    inline const TValue& Variant::get_value() const
    {
        return std::any_cast<const TValue&>(_storage);
    }

    inline Variant& Variant::operator=(const Variant&) = default;

    inline Variant& Variant::operator=(Variant&&) noexcept = default;

    template <typename TValue, typename>
    inline Variant& Variant::operator=(TValue&& value)
    {
        _storage = std::forward<TValue>(value);
        return *this;
    }
}
