#pragma once
#include "Tbx/DllExport.h"
#include <string>

namespace Tbx
{
    struct TBX_EXPORT Text
    {
        Text() = default;
        Text(const std::string& value, const std::string& font, int size)
            : Value(value), Font(font), Size(size) {}

        std::string Value = "";
        std::string Font = "";
        int Size = 12;
    };
}