#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Debug/IPrintable.h"
#include "Tbx/Math/Int.h"
#include <string>

namespace Tbx
{
    struct TBX_EXPORT Uid : public IPrintable
    {
        Uid() = default;
        Uid(uint64 value) : Value(value) {}

        std::string ToString() const override { return std::to_string(Value); }
        static Uid Generate();

        explicit(false) operator uint64() const { return Value; }

        static Uid Invalid;
        uint64 Value = -1;
    };
}

// Specialize std::hash for UID
namespace std
{
    template <>
    struct TBX_EXPORT hash<Tbx::Uid>
    {
        std::size_t operator()(const Tbx::Uid& uid) const
        {
            return std::hash<Tbx::uint64>{}(uid.Value);
        }
    };
}

// Specialize std::equal_to for UID
namespace std
{
    template <>
    struct TBX_EXPORT equal_to<Tbx::Uid>
    {
        bool operator()(const Tbx::Uid& lhs, const Tbx::Uid& rhs) const
        {
            return lhs.Value == rhs.Value;
        }
    };
}
