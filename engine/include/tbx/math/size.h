#pragma once
#include "tbx/tbx_api.h"
#include <cstdint>

namespace tbx::math
{
    struct TBX_API Size
    {
        std::int32_t width = 0;
        std::int32_t height = 0;
    };
}
