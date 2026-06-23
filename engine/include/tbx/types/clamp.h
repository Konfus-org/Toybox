#pragma once
#include <algorithm>
#include <limits>

namespace tbx
{
    /// @brief
    /// Purpose: A numeric value that always stays within compile-time [Min, Max] bounds. Constructing
    /// or assigning from an out-of-range value clamps it; reading converts implicitly to the
    /// underlying T. Specify only a minimum (leave Max defaulted), only a maximum (pass the type's
    /// lowest as Min), or both.
    /// @details
    /// Ownership: Value type holding a single T. Thread Safety: Safe to copy between threads.
    /// Serialization: Treated as a transparent wrapper — it serializes and presents to the editor
    /// exactly as its underlying T (see IsClamp handling in serialization.h), so swapping a plain T
    /// field for a Clamp<T> is backwards compatible on disk.
    template <
        typename T,
        T Min = std::numeric_limits<T>::lowest(),
        T Max = std::numeric_limits<T>::max()>
    struct Clamp
    {
        static_assert(Min <= Max, "Clamp bounds must satisfy Min <= Max.");

        T value = std::clamp(T {}, Min, Max);

        constexpr Clamp() = default;

        constexpr explicit(false) Clamp(T initial)
            : value(std::clamp(initial, Min, Max))
        {
        }

        constexpr Clamp& operator=(T next)
        {
            value = std::clamp(next, Min, Max);
            return *this;
        }

        constexpr explicit(false) operator const T&() const
        {
            return value;
        }

        static constexpr T minimum()
        {
            return Min;
        }

        static constexpr T maximum()
        {
            return Max;
        }

        bool operator==(const Clamp& other) const = default;
    };
}
