#pragma once
#include <array>
#include <cstdint>

namespace tbx
{
    struct Uuid
    {
        // Generate a randomly initialized UUID (RFC 4122 variant, random-based)
        static Uuid generate();

        // Returns true if this UUID is non-zero (not the all-zero value)
        bool is_valid() const noexcept;

        friend bool operator==(const Uuid& a, const Uuid& b) noexcept { return a.bytes == b.bytes; }
        friend bool operator!=(const Uuid& a, const Uuid& b) noexcept { return !(a == b); }

        std::array<std::uint8_t, 16> bytes = {};
    };

    struct UuidHash
    {
        std::size_t operator()(const Uuid& id) const noexcept;
    };
}
