#pragma once
#include "tbx/tbx_api.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace tbx
{
    struct TBX_API Uuid
    {
        Uuid() = default;
        Uuid(std::uint32_t v)
            : value(v)
        {
        }

        static Uuid generate();
        bool is_valid() const;

        operator bool() const;
        operator std::uint32_t() const;
        operator std::string() const;
        bool operator!() const;
        bool operator<(const Uuid& other) const;
        bool operator>(const Uuid& other) const;
        bool operator<=(const Uuid& other) const;
        bool operator>=(const Uuid& other) const;
        bool operator==(const Uuid& other) const;
        bool operator!=(const Uuid& other) const;

        std::uint32_t value = 0U;
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
            return hash<std::uint32_t>()(static_cast<const std::uint32_t&>(value));
        }
    };
}
