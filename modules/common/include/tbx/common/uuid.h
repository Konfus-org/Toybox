#pragma once
#include "tbx/std/array.h"
#include "tbx/std/string.h"
#include "tbx/std/int.h"
#include "tbx/tbx_api.h"
#include <cstdint>

namespace tbx
{
    struct TBX_API Uuid
    {
        static Uuid generate();
        bool is_valid() const;

        Array<std::uint8_t, 16> bytes = {};
    };

    TBX_API bool operator==(const Uuid& a, const Uuid& b);
    TBX_API bool operator!=(const Uuid& a, const Uuid& b);

    struct UuidHash
    {
        uint64 operator()(const Uuid& id) const;
    };

    TBX_API String to_string(const Uuid& id);
}
