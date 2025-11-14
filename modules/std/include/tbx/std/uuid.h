#pragma once
#include "tbx/tbx_api.h"
#include "tbx/std/int.h"
#include <array>
#include <cstdint>
#include <string>

namespace tbx
{
    struct TBX_API Uuid
    {
        static Uuid generate();
        bool is_valid() const;

        std::array<std::uint8_t, 16> bytes = {};
    };

    TBX_API bool operator==(const Uuid& a, const Uuid& b);
    TBX_API bool operator!=(const Uuid& a, const Uuid& b);

    struct UuidHash
    {
        uint64 operator()(const Uuid& id) const;
    };

    TBX_API std::string to_string(const Uuid& id);
}
