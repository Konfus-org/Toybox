#pragma once
#include "tbx/common/int.h"
#include "tbx/tbx_api.h"
#include <cstddef>
#include <functional>
#include <string>

namespace tbx
{
    struct Uuid
    {
        TBX_API Uuid();
        TBX_API Uuid(uint32 v);

        TBX_API static Uuid generate();

        /// <summary>
        /// Purpose: Combines a base UUID with an additional value into a new UUID.
        /// </summary>
        /// <remarks>
        /// Ownership: Returns a value type; no ownership transfer.
        /// Thread Safety: Safe to call concurrently.
        /// </remarks>
        TBX_API static Uuid combine(Uuid base, uint32 value);

        /// <summary>
        /// Purpose: Combines an additional value into this UUID.
        /// </summary>
        /// <remarks>
        /// Ownership: Mutates this UUID in place.
        /// Thread Safety: Not thread-safe; synchronize mutation externally.
        /// </remarks>
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

    inline const Uuid Uuid::NONE = {};

    /// <summary>Purpose: Formats a UUID value as a hex string.</summary>
    /// <remarks>Ownership: Returns an owned std::string. Thread Safety: Stateless and safe for
    /// concurrent use.</remarks>
    TBX_API std::string to_string(const Uuid& value);
}

namespace std
{
    template <>
    struct hash<tbx::Uuid>
    {
        std::size_t operator()(const tbx::Uuid& value) const
        {
            return hash<tbx::uint32>()(static_cast<tbx::uint32>(value));
        }
    };
}
