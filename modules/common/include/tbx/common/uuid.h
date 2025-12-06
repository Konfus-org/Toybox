#pragma once
#include "tbx/common/int.h"
#include "tbx/tbx_api.h"
#include <cstdint>
#include <string>

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
        bool operator!() const;
        bool operator<(const Uuid& other) const;
        bool operator>(const Uuid& other) const;
        bool operator<=(const Uuid& other) const;
        bool operator>=(const Uuid& other) const;
        bool operator==(const Uuid& other) const;
        bool operator!=(const Uuid& other) const;
        operator uint32() const;

        uint32 value = 0U;
    };

    namespace invalid
    {
        inline Uuid uuid = Uuid();
    }

    struct UuidHash
    {
        uint32 operator()(const Uuid& id) const;
    };

    TBX_API std::string to_string(const Uuid& id);
}
