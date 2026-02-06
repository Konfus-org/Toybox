#pragma once
#include "tbx/tbx_api.h"
#include <string>
#include <utility>

namespace tbx
{
    struct TBX_API Text
    {
        Text() = default;
        Text(std::string value, std::string font, int size)
            : value(std::move(value))
            , font(std::move(font))
            , size(size)
        {
        }

        std::string value = "";
        std::string font = "";
        int size = 12;
    };
}
