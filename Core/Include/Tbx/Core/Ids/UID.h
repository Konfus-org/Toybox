#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Math/Int.h"
#include <string>

namespace Tbx
{
    struct EXPORT UID
    {
    public:
        /// <summary>
        /// Will generate a new UID
        /// </summary>
        UID() : _value(GetNextId()) {}

        explicit(false) UID(uint64 id) : _value(id) {}
        explicit(false) UID(uint id) : _value(static_cast<uint64>(id)) {}
        explicit(false) UID(int id) : _value(static_cast<uint64>(id)) {}

        const uint64& GetValue() const { return _value; }

        std::string ToString() const { return std::to_string(_value); }

        explicit(false) operator uint64() const { return _value; }

    private:
        static uint64 GetNextId();

        uint64 _value = 0;
    };
}
