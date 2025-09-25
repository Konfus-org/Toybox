#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Debug/IPrintable.h"
#include <string>

namespace Tbx
{
    struct TBX_EXPORT Guid : public IPrintable
    {
        Guid() = default;
        Guid(const std::string& value) : Value(value) {}

        static Guid Generate();
        std::string ToString() const override { return Value; }

        bool operator==(const Guid& other) const { return Value == other.Value; }

        static Guid Invalid;
        std::string Value = "00000000-0000-0000-0000-000000000000";
    };
}

// Specialize std::hash for GUID
namespace std
{
    template <>
    struct TBX_EXPORT hash<Tbx::Guid>
    {
        std::size_t operator()(const Tbx::Guid& guid) const
        {
            return std::hash<std::string>{}(guid.Value);
        }
    };
}

// Specialize std::equal_to for GUID
namespace std
{
    template <>
    struct TBX_EXPORT equal_to<Tbx::Guid>
    {
        bool operator()(const Tbx::Guid& lhs, const Tbx::Guid& rhs) const
        {
            return lhs.Value == rhs.Value;
        }
    };
}
