#pragma once
#include "tbx/core/typedefs.h"
#include <string_view>

namespace tbx
{
    /// @brief
    /// Purpose: FNV-1a 64-bit name hash — the engine's stable identity for registered type and
    /// sticker names (usable at compile time).
    constexpr uint64 hash_name(std::string_view text)
    {
        uint64 hash = 14695981039346656037ull;
        for (const char c : text)
        {
            hash ^= static_cast<uint64>(static_cast<unsigned char>(c));
            hash *= 1099511628211ull;
        }
        return hash;
    }
}
