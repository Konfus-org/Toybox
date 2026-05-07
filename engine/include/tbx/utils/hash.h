#pragma once
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"

namespace tbx
{
    inline constexpr uint64 TBX_FNV1A_OFFSET_BASIS = 14695981039346656037ULL;
    inline constexpr uint64 TBX_FNV1A_PRIME = 1099511628211ULL;

    inline uint64 fnv1a_hash_bytes(
        const void* data,
        const uint64 data_size,
        uint64 hash = TBX_FNV1A_OFFSET_BASIS)
    {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (uint64 index = 0U; index < data_size; ++index)
        {
            hash ^= bytes[index];
            hash *= TBX_FNV1A_PRIME;
        }
        return hash;
    }

    inline uint64 fnv1a_hash_value(const uint64 value, const uint64 hash)
    {
        return fnv1a_hash_bytes(&value, static_cast<uint64>(sizeof(value)), hash);
    }

    inline uint64 fnv1a_hash_uuid(const Uuid value, const uint64 hash)
    {
        return fnv1a_hash_value(static_cast<uint32>(value), hash);
    }
}
