#pragma once
#include "tbx/common/int.h"
#include "tbx/common/string.h"
#include "tbx/tbx_api.h"
#include <cstdint>
#include <string>
#include <ostream>

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
        operator String() const;

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

    // Streams the hexadecimal representation of the UUID to the provided output stream.
    TBX_API std::ostream& operator<<(std::ostream& stream, const Uuid& id);
    TBX_API String to_string(const Uuid& id);
}
