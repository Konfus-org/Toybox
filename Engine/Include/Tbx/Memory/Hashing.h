#pragma once
#include "Tbx/Math/Int.h"
#include <typeinfo>

namespace Tbx::Memory
{
    template <typename TValue>
    inline constexpr uint64 Hash();

    inline constexpr uint64 Hash(const std::type_info& info)
    {
        return static_cast<uint64>(info.hash_code());
    }

    template <typename TValue>
    inline constexpr uint64 Hash()
    {
        return Hash(typeid(TValue));
    }

    template <typename TValue>
    inline uint64 Hash(const TValue& value)
    {
        return Hash(typeid(value));
    }

    /// <summary>
    /// Combines two 64-bit hash values into a single, well-distributed hash.
    /// </summary>
    /// <param name="lhs">The first hash value.</param>
    /// <param name="rhs">The second hash value.</param>
    /// <returns>A deterministic hash that mixes both input hashes.</returns>
    inline constexpr uint64 CombineHashes(uint64 lhs, uint64 rhs)
    {
        constexpr uint64 HASH_COMBINE_MAGIC = 0x9e3779b97f4a7c15ULL;
        lhs ^= rhs + HASH_COMBINE_MAGIC + (lhs << 6U) + (lhs >> 2U);
        return lhs;
    }
}

