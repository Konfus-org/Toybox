#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <string_view>

namespace tbx
{
    [[tbx::serializable]];
    [[tbx::printable("{:x}", value)]];
    [[tbx::hash(value)]];
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

        [[tbx::prop]]
        uint32 value = 0U;

        static const Uuid NONE;
    };

    inline const Uuid Uuid::NONE = {};

    /// @brief Purpose: Parses a UUID value from a hex string.
    TBX_API Uuid parse_uuid(std::string_view value);

    /// @brief Purpose: Hashes a string to produce a UUID value.
    TBX_API Uuid hash_string_to_id(std::string_view handle_name);
}

#include "tbx/types/uuid.generated.h"
