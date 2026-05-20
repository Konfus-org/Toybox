#pragma once
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include <string>
#include <string_view>
#include <type_traits>

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

    inline uint64 hash(
        const void* data,
        const uint64 data_size,
        uint64 value = TBX_FNV1A_OFFSET_BASIS)
    {
        return fnv1a_hash_bytes(data, data_size, value);
    }

    template <typename TValue>
    inline uint64 hash(const TValue& data, const uint64 value = TBX_FNV1A_OFFSET_BASIS)
    {
        static_assert(std::is_trivially_copyable_v<TValue>);
        return hash(&data, static_cast<uint64>(sizeof(data)), value);
    }

    inline uint64 hash(const std::string_view data, const uint64 value = TBX_FNV1A_OFFSET_BASIS)
    {
        return hash(data.data(), static_cast<uint64>(data.size()), value);
    }

    inline uint64 hash(const std::string& data, const uint64 value = TBX_FNV1A_OFFSET_BASIS)
    {
        return hash(std::string_view(data), value);
    }

    inline uint64 hash(const Uuid data, const uint64 value = TBX_FNV1A_OFFSET_BASIS)
    {
        return hash(static_cast<uint64>(static_cast<uint32>(data)), value);
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
