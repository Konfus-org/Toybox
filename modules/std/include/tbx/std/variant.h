#pragma once
#include "tbx/tbx_api.h"
#include <any>
#include <type_traits>
#include <utility>

namespace tbx
{
    /// Type-erased value container built atop `std::any`.
    /// Ownership: Stores a copy of the assigned value within the variant instance.
    /// Thread-safety: Not thread-safe; callers must serialize access when sharing instances.
    class TBX_API Variant
    {
      public:
        Variant();

        template <typename TValue, typename = std::enable_if_t<!std::is_same_v<std::decay_t<TValue>, Variant>>>
        Variant(TValue&& value);

        Variant(const Variant&);
        Variant(Variant&&) noexcept;
        ~Variant();

        bool has_value() const;

        bool is_empty() const;

        void reset();

        template <typename TValue>
        bool is() const;

        template <typename TValue>
        TValue& get_value();

        template <typename TValue>
        const TValue& get_value() const;

        Variant& operator=(const Variant&);
        Variant& operator=(Variant&&) noexcept;

        template <typename TValue, typename = std::enable_if_t<!std::is_same_v<std::decay_t<TValue>, Variant>>>
        Variant& operator=(TValue&& value);

      private:
        std::any _storage;
    };
}

#include "../../../src/variant.inl"
