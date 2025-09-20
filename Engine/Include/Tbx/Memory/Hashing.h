#pragma once
#include "Tbx/Math/Int.h"
#include <cstdint>

namespace Tbx::Memory
{
    /// <summary>
    /// Combines two 64-bit hash values into a single, well-distributed hash.
    /// </summary>
    /// <param name="lhs">The first hash value.</param>
    /// <param name="rhs">The second hash value.</param>
    /// <returns>A deterministic hash that mixes both input hashes.</returns>
    constexpr Tbx::uint64 CombineHashes(Tbx::uint64 lhs, Tbx::uint64 rhs)
    {
        constexpr Tbx::uint64 HASH_COMBINE_MAGIC = 0x9e3779b97f4a7c15ULL;
        lhs ^= rhs + HASH_COMBINE_MAGIC + (lhs << 6U) + (lhs >> 2U);
        return lhs;
    }
}

