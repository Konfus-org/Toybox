#pragma once
#include "tbx/tbx_api.h"
#include <array>
#include <cstdint>
#include <string>

namespace tbx
{
    struct TBX_API Uuid
    {
        // Generate a randomly initialized UUID (RFC 4122 variant, random-based)
        static Uuid generate();

        // Returns true if this UUID is non-zero (not the all-zero value)
        bool is_valid() const;

        friend bool operator==(const Uuid& a, const Uuid& b)
        {
            return a.bytes == b.bytes;
        }

        friend bool operator!=(const Uuid& a, const Uuid& b)
        {
            return !(a == b);
        }

        std::array<std::uint8_t, 16> bytes = {};
    };

    struct UuidHash
    {
        std::size_t operator()(const Uuid& id) const;
    };

    inline std::string to_string(const Uuid& id)
    {
        static const char* hex = "0123456789abcdef";
        std::string s;
        s.reserve(36); // canonical UUID length

        for (std::size_t i = 0; i < id.bytes.size(); ++i)
        {
            // insert hyphens at positions: after bytes 3,5,7,9
            if (i == 4 || i == 6 || i == 8 || i == 10)
            {
                s.push_back('-');
            }

            unsigned char byte = id.bytes[i];
            s.push_back(hex[(byte >> 4) & 0x0F]);
            s.push_back(hex[byte & 0x0F]);
        }

        return s;
    }
}
