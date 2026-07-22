#include "tbx/utils/uuid.h"
#include <charconv>
#include <random>

namespace tbx
{
    //// HELPERS ////

    static uint64 random_seed()
    {
        auto device = std::random_device();
        return (static_cast<uint64>(device()) << 32) ^ device();
    }

    //// UUID ////

    Uuid Uuid::generate()
    {
        thread_local std::mt19937_64 rng(random_seed());
        auto id = Uuid {.hi = rng(), .lo = rng()};
        if (!id.is_valid())
            id.lo = 1;
        return id;
    }

    std::string Uuid::to_string() const
    {
        char buffer[33] = {};
        auto write_hex = [](uint64 value, char* out)
        {
            for (int i = 15; i >= 0; --i)
            {
                out[i] = "0123456789abcdef"[value & 0xF];
                value >>= 4;
            }
        };
        write_hex(hi, buffer);
        write_hex(lo, buffer + 16);
        return std::string(buffer, 32);
    }

    Uuid Uuid::parse(const std::string& text)
    {
        if (text.size() != 32)
            return {};
        auto id = Uuid {};
        auto hi_result = std::from_chars(text.data(), text.data() + 16, id.hi, 16);
        auto lo_result = std::from_chars(text.data() + 16, text.data() + 32, id.lo, 16);
        bool parsed = hi_result.ec == std::errc {} && lo_result.ec == std::errc {}
            && hi_result.ptr == text.data() + 16 && lo_result.ptr == text.data() + 32;
        if (!parsed)
            return {};
        return id;
    }
}
