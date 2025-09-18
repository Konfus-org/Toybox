#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TypeAliases/Int.h"
#include <string>

namespace Tbx
{
    struct EXPORT Guid
    {
        // Generates a new GUID of the format 00000000-0000-0000-0000-000000000000
        static Guid Generate();

        static Tbx::Guid Invalid;

        std::string Value = "00000000-0000-0000-0000-000000000000";
    };
}

// Specialize std::hash for GUID
namespace std
{
    template <>
    struct EXPORT hash<Tbx::Guid>
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
    struct EXPORT equal_to<Tbx::Guid>
    {
        bool operator()(const Tbx::Guid& lhs, const Tbx::Guid& rhs) const
        {
            return lhs.Value == rhs.Value;
        }
    };
}