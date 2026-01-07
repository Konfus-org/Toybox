#pragma once
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    struct TBX_API Text
    {
        Text() = default;
        Text(const std::string& value, const std::string& font, int size)
            : value(value)
            , font(font)
            , size(size)
        {
        }

        std::string value = "";
        std::string font = "";
        int size = 12;
    };
}
