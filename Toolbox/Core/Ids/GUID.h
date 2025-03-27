#pragma once
#include "TbxPCH.h"
#include "TbxAPI.h"
#include "Math/Int.h"


namespace Tbx
{
    class GUID
    {
    public:
        TBX_API GUID() = default;
        TBX_API explicit(false) GUID(const std::string& uuid) : _value(uuid) {}

        TBX_API std::string GetValue() const { return _value; }
        TBX_API explicit(false) operator std::string() const { return GetValue(); }

        TBX_API bool operator==(const GUID& other) const = default;

        TBX_API static GUID Generate()
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
}