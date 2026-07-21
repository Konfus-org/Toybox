#pragma once
#include "tbx/utils/typedefs.h"
#include "tbx/utils/uuid.h"
#include <string_view>

namespace tbx
{
    /// @brief
    /// Purpose: THE hash — one overload set for anything the engine hashes; call
    /// tbx::hash(whatever). Strings use FNV-1a 64 (stable, usable at compile time).
    constexpr uint64 hash(const std::string_view text)
    {
        uint64 value = 14695981039346656037ull;
        for (const char c : text)
        {
            value ^= static_cast<uint64>(static_cast<unsigned char>(c));
            value *= 1099511628211ull;
        }
        return value;
    }

    /// @brief
    /// Purpose: Hashes a uuid identity.
    constexpr uint64 hash(const Uuid& id)
    {
        return id.hi ^ (id.lo * 0x9E3779B97F4A7C15ull);
    }

    /// @brief
    /// Purpose: Integers hash as themselves (mixing is the container's concern).
    constexpr uint64 hash(const uint64 value)
    {
        return value;
    }
}
