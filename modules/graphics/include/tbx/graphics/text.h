#pragma once
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    struct TBX_API Text
    {
        Text() = default;
        Text(const String& value, const String& font, int size)
            : value(value), font(font), size(size) {}

        String value = "";
        String font = "";
        int size = 12;
    };
}
