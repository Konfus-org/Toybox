#pragma once
#include "tbx/tbx_api.h"
#include <cstdint>

namespace tbx
{
    struct TBX_API Size
    {
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };
}
