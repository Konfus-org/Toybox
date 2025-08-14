#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TypeAliases/Int.h"
#include <string>
#include <random>
#include <format>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace Tbx
{
    class Guid
    {
    public:
        // Explicitly sets the GUID to a specific value
        EXPORT explicit(false) Guid(const std::string& guid) : _value(guid) {}
        // Implicitly sets the GUID to the default value (00000000-0000-0000-0000-000000000000)
        EXPORT Guid() = default;

        EXPORT std::string GetValue() const { return _value; }
        EXPORT explicit(false) operator std::string() const { return GetValue(); }

        EXPORT bool operator==(const Guid& other) const = default;

        // Generates a new GUID of the format 00000000-0000-0000-0000-000000000000
        EXPORT static Guid Generate()
        {
            static std::random_device rd;
            static std::mt19937_64 gen(rd());
            static std::uniform_int_distribution<uint64> dist(0, 0xFFFFFFFFFFFFFFFFULL);

            uint64 part1 = dist(gen);
            uint64 part2 = dist(gen);

            // Adjust bits for version 4 UUID
            part1 = (part1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL; // Version 4
            part2 = (part2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL; // Variant

            std::string uuid = std::format("{:08x}-{:04x}-{:04x}-{:04x}-{:012x}",
                (part1 >> 32),
                ((part1 >> 16) & 0xFFFF),
                (part1 & 0xFFFF),
                (part2 >> 48),
                (part2 & 0xFFFFFFFFFFFFULL));

            return uuid;
        }
        
    private:
        std::string _value = "00000000-0000-0000-0000-000000000000";
    };

    namespace Invalid
    {
        static inline Tbx::Guid Guid = Tbx::Guid("00000000-0000-0000-0000-000000000000");
    }
}

// Specialize std::hash for GUID
namespace std
{
    template <>
    struct hash<Tbx::Guid>
    {
        std::size_t operator()(const Tbx::Guid& guid) const
        {
            return std::hash<std::string>{}(guid.GetValue());
        }
    };
}

// Specialize std::equal_to for GUID
namespace std
{
    template <>
    struct equal_to<Tbx::Guid>
    {
        bool operator()(const Tbx::Guid& lhs, const Tbx::Guid& rhs) const
        {
            return lhs.GetValue() == rhs.GetValue();
        }
    };
}