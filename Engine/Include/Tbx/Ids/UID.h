#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TypeAliases/Int.h"
#include <string>

namespace Tbx
{
    struct EXPORT Uid
    {
    public:
        /// <summary>
        /// Will generate a new UID
        /// </summary>
        Uid() : Value(GetNextId()) {}
        explicit(false) Uid(uint64 id) : Value(id) {}
        explicit(false) Uid(uint id) : Value(static_cast<uint64>(id)) {}
        explicit(false) Uid(int id) : Value(static_cast<uint64>(id)) {}

        std::string ToString() const { return std::to_string(Value); }

        explicit(false) operator uint64() const { return Value; }

        static uint64 GetNextId();

        uint64 Value = 0;
    };

    namespace Consts::Invalid
    {
        EXPORT inline Tbx::Uid Uid = -1;
    }
}

// Specialize std::hash for UID
namespace std
{
    template <>
    struct hash<Tbx::Uid>
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
    struct equal_to<Tbx::Uid>
    {
        bool operator()(const Tbx::Uid& lhs, const Tbx::Uid& rhs) const
        {
            return lhs.Value == rhs.Value;
        }
    };
}