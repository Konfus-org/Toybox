#pragma once
#include "casting.h"
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
        template <typename TValue>
        Variant(void* value)
            : _storage(std::forward<TValue>(value))
        {
        }
        Variant(const Variant&) = default;
        Variant(Variant&&) noexcept = default;

        bool has_value() const
        {
            return _storage.has_value();
        }

        const std::type_info& get_type() const
        {
            return _storage.type();
        }

        void reset()
        {
            _storage.reset();
        }

        template <typename TValue>
        TValue& get_value()
        {
            auto* ptr = try_as<TValue>();
            if (ptr == nullptr)
            {
                throw std::bad_cast();
            }
            return *ptr;
        }

        template <typename TValue>
        const TValue& get_value() const
        {
            auto* ptr = try_as<TValue>();
            return *ptr;
        }

        Variant& operator=(const Variant&) = default;
        Variant& operator=(Variant&&) noexcept = default;

        template <typename TValue>
        Variant& operator=(TValue&& value)
        {
            _storage = std::forward<TValue>(value);
            return *this;
        }

      private:
        std::any _storage;
    };
}
