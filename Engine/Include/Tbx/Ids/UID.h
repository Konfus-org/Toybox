#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Type Aliases/Int.h"
#include <string>

namespace Tbx
{
    struct EXPORT UID
    {
    public:
        /// <summary>
        /// Will generate a new UID
        /// </summary>
        UID() : Value(GetNextId()) {}
        explicit(false) UID(uint64 id) : Value(id) {}
        explicit(false) UID(uint id) : Value(static_cast<uint64>(id)) {}
        explicit(false) UID(int id) : Value(static_cast<uint64>(id)) {}

        std::string ToString() const { return std::to_string(Value); }

        explicit(false) operator uint64() const { return Value; }

        static uint64 GetNextId();

        uint64 Value = 0;
    };

    static inline UID INVALID_UID = -1;
}
