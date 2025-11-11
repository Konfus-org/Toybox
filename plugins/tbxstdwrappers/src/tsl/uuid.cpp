#include "tbx/tsl/uuid.h"
#include <random>

namespace tbx
{
    Uuid Uuid::generate()
    {
        Uuid id = {};

        // Fill with random bytes
        std::random_device rd;
        std::mt19937_64 gen(rd());
        for (uint i = 0; i < static_cast<uint>(id.bytes.size()); i += 8)
        {
            const auto r = gen();
            const uint remaining = static_cast<uint>(id.bytes.size()) - i;
            const uint count = remaining < 8 ? remaining : 8;
            for (uint j = 0; j < count; ++j)
            {
                id.bytes[i + j] = static_cast<std::uint8_t>((r >> (j * 8)) & 0xFF);
            }
        }

        // RFC 4122 variant and version (random-based)
        id.bytes[6] =
            static_cast<std::uint8_t>((id.bytes[6] & 0x0F) | 0x40); // version 4 in the high nibble
        id.bytes[8] = static_cast<std::uint8_t>((id.bytes[8] & 0x3F) | 0x80); // variant 1 (10xx)

        return id;
    }

    bool Uuid::is_valid() const
    {
        for (std::uint8_t b : bytes)
        {
            if (b != 0)
                return true;
        }
        return false;
    }

    uint64 UuidHash::operator()(const Uuid& id) const
    {
        // Combine two 64-bit halves
        std::uint64_t a = 0, b = 0;
        for (int i = 0; i < 8; ++i)
            a = (a << 8) | id.bytes[static_cast<uint>(i)];
        for (int i = 8; i < 16; ++i)
            b = (b << 8) | id.bytes[static_cast<uint>(i)];
        return a ^ (b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2));
    }
}
