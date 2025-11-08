#pragma once
#include "tbx/tbx_api.h"
#include "tbx/tsl/int.h"
#include <array>
#include <cstdint>
#include <string>

namespace tbx
{
    struct TBX_API Uuid
    {
        static Uuid generate();
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
        uint64 operator()(const Uuid& id) const;
    };

    inline std::string to_string(const Uuid& id)
    {
        static const char* hex = "0123456789abcdef";
        std::string s;
        s.reserve(36);

        for (uint i = 0; i < static_cast<uint>(id.bytes.size()); ++i)
        {
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
