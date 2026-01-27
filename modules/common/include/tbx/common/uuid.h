#pragma once
#include "tbx/common/int.h"
#include "tbx/tbx_api.h"
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace tbx
{
    struct TBX_API Uuid
    {
        Uuid() = default;
        Uuid(uint32 v)
            : value(v)
        {
        }

        static Uuid generate();
        bool is_valid() const;

        operator bool() const;
        operator uint32() const;
        bool operator!() const;
        bool operator<(const Uuid& other) const;
        bool operator>(const Uuid& other) const;
        bool operator<=(const Uuid& other) const;
        bool operator>=(const Uuid& other) const;
        bool operator==(const Uuid& other) const;
        bool operator!=(const Uuid& other) const;

        uint32 value = 0U;
    };

    /// <summary>Purpose: Formats a UUID value as a hex string.</summary>
    /// <remarks>Ownership: Returns an owned std::string. Thread Safety: Stateless and safe for
    /// concurrent use.</remarks>
    TBX_API std::string to_string(const Uuid& value);

    /// <summary>Purpose: Parses a UUID value from a text representation.</summary>
    /// <remarks>Ownership: Returns a value-type UUID. Thread Safety: Stateless and safe for
    /// concurrent use.</remarks>
    TBX_API Uuid from_string(std::string_view value);

    namespace invalid
    {
        inline Uuid uuid = Uuid();
    }

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
