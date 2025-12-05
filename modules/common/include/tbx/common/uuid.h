#pragma once
#include "tbx/common/int.h"
#include "tbx/tbx_api.h"
#include <cstdint>
#include <string>

namespace tbx
{
    struct TBX_API Uuid
    {
        static Uuid generate();
        bool is_valid() const;

        std::uint64_t value = 0U;
    };

    namespace invalid
    {
        inline Uuid uuid = Uuid();
    }

    TBX_API bool operator==(const Uuid& a, const Uuid& b);
    TBX_API bool operator!=(const Uuid& a, const Uuid& b);

    struct UuidHash
    {
        uint64 operator()(const Uuid& id) const;
    };

    TBX_API std::string to_string(const Uuid& id);
}
