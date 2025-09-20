#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Core/StringConvertible.h"
#include <string>
#include <string_view>
#include <utility>

namespace Tbx
{
    struct EXPORT Guid : public IStringConvertible
    {
        Guid() = default;
        explicit(false) Guid(const std::string& value) : Value(value) {}
        explicit(false) Guid(std::string&& value) : Value(std::move(value)) {}
        explicit(false) Guid(std::string_view value) : Value(value) {}
        explicit(false) Guid(const char* value) : Value(value) {}

        bool operator==(const Guid& other) const { return Value == other.Value; }

        // Generates a new GUID of the format 00000000-0000-0000-0000-000000000000
        static Guid Generate();
        std::string ToString() const override { return Value; }

        static Guid Invalid;
        std::string Value = Invalid;
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
