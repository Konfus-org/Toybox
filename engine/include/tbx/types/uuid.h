#pragma once
#include "tbx/systems/files/serialization.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <cstddef>
#include <format>
#include <functional>
#include <string>
#include <string_view>

namespace tbx
{
    struct Uuid
    {
        TBX_API Uuid();
        TBX_API Uuid(uint32 v);

        TBX_API static Uuid generate();

        /// @brief
        /// Purpose: Combines a base UUID with an additional value into a new UUID.
        /// @details
        /// Ownership: Returns a value type; no ownership transfer.
        /// Thread Safety: Safe to call concurrently.
        TBX_API static Uuid combine(Uuid base, uint32 value);

        /// @brief
        /// Purpose: Combines an additional value into this UUID.
        /// @details
        /// Ownership: Mutates this UUID in place.
        /// Thread Safety: Not thread-safe; synchronize mutation externally.
        TBX_API void combine(uint32 value);

        TBX_API bool is_valid() const;

        TBX_API operator bool() const;
        TBX_API operator uint32() const;
        TBX_API bool operator!() const;
        TBX_API bool operator<(const Uuid& other) const;
        TBX_API bool operator>(const Uuid& other) const;
        TBX_API bool operator<=(const Uuid& other) const;
        TBX_API bool operator>=(const Uuid& other) const;
        TBX_API bool operator==(const Uuid& other) const;
        TBX_API bool operator!=(const Uuid& other) const;

        uint32 value = 0U;

        static const Uuid NONE;
    };

    TBX_SERIALIZABLE_STRUCT(Uuid, value)

    inline const Uuid Uuid::NONE = {};

    /// @brief Purpose: Parses a UUID value from a hex string.
    TBX_API Uuid parse_uuid(std::string_view value);

    /// @brief Purpose: Hashes a string to produce a UUID value.
    TBX_API Uuid hash_string_to_id(std::string_view handle_name);
}

template <>
struct std::formatter<tbx::Uuid>
{
    constexpr auto parse(std::format_parse_context& ctx)
    {
        return _formatter.parse(ctx);
    }

    template <typename TFormatContext>
    auto format(const tbx::Uuid& value, TFormatContext& ctx) const
    {
        return _formatter.format(std::format("{:x}", value.value), ctx);
    }

    std::formatter<std::string> _formatter;
};

template <>
struct std::hash<tbx::Uuid>
{
    ::size operator()(const tbx::Uuid& value) const;
};

#include "tbx/types/uuid.inl"
