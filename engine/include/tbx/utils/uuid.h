#pragma once
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include <functional>
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: 128-bit random identity, stable across runs (serialized into kits), unlike Toy
    /// ids which are per-session registry handles.
    struct TBX_API Uuid
    {
        uint64 hi = 0;
        uint64 lo = 0;

        /// @brief
        /// Purpose: Creates a new random (non-nil) uuid.
        static Uuid generate();

        /// @brief
        /// Purpose: Parses a 32-char lowercase-hex uuid; returns a nil uuid on malformed input.
        static Uuid parse(const std::string& text);

        /// @brief
        /// Purpose: Reports whether this is the all-zero "no identity" value.
        bool is_nil() const
        {
            return hi == 0 && lo == 0;
        }

        /// @brief
        /// Purpose: Formats as 32 lowercase hex chars — the inverse of parse().
        std::string to_string() const;

        auto operator<=>(const Uuid&) const = default;
    };
}

template <>
struct std::hash<tbx::Uuid>
{
    // ::size, not bare size — inside namespace std the latter finds std::size().
    ::size operator()(const tbx::Uuid& id) const noexcept
    {
        return id.hi ^ (id.lo * 0x9E3779B97F4A7C15ull);
    }
};
