#pragma once
#include "tbx/tbx_api.h"
#include <any>
#include <type_traits>
#include <utility>

namespace tbx
{
    class TBX_API Variant
    {
      public:
        Variant() = default;

        template <typename TValue, typename = std::enable_if_t<!std::is_same_v<std::decay_t<TValue>, Variant>>>
        Variant(TValue&& value)
        {
            _storage = std::forward<TValue>(value);
        }

        Variant(const Variant&) = default;
        Variant(Variant&&) noexcept = default;
        ~Variant() = default;

        bool has_value() const
        {
            return _storage.has_value();
        }

        bool is_empty() const
        {
            return !has_value();
        }

        void reset()
        {
            _storage.reset();
        }

        template <typename TValue>
        bool is() const
        {
            using TValueNoRef = std::remove_cv_t<std::remove_reference_t<TValue>>;
            return std::any_cast<TValueNoRef>(&_storage) != nullptr;
        }

        template <typename TValue>
        TValue& get_value()
        {
            return std::any_cast<TValue&>(_storage);
        }

        template <typename TValue>
        const TValue& get_value() const
        {
            return std::any_cast<const TValue&>(_storage);
        }

        Variant& operator=(const Variant&) = default;
        Variant& operator=(Variant&&) noexcept = default;

        template <typename TValue, typename = std::enable_if_t<!std::is_same_v<std::decay_t<TValue>, Variant>>>
        Variant& operator=(TValue&& value)
        {
            _storage = std::forward<TValue>(value);
            return *this;
        }

      private:
        std::any _storage;
    };
}
