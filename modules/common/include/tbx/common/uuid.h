#pragma once
#include "tbx/common/int.h"
#include "tbx/common/string.h"
#include "tbx/tbx_api.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ostream>
#include <string>

namespace tbx
{
    struct TBX_API Uuid
    {
        Uuid() = default;
        Uuid(uint v)
            : value(v)
        {
        }

        static Uuid generate();
        bool is_valid() const;

        operator bool() const;
        operator uint() const;
        operator String() const;
        bool operator!() const;
        bool operator<(const Uuid& other) const;
        bool operator>(const Uuid& other) const;
        bool operator<=(const Uuid& other) const;
        bool operator>=(const Uuid& other) const;
        bool operator==(const Uuid& other) const;
        bool operator!=(const Uuid& other) const;

        uint value = 0U;
    };

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
        size_t operator()(const tbx::Uuid& value) const
        {
            return hash<tbx::uint>()(static_cast<const tbx::uint&>(value));
        }
    };
}
