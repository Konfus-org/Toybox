#pragma once
#include "tbx/interfaces/graphics_backend.h"

namespace tbx
{
    struct GraphicsBufferUpdateRequest
    {
        const void* data = nullptr;
        uint64 data_size = 0U;
        uint64 offset = 0U;
    };

    struct GraphicsTextureUpdateRequest
    {
        GraphicsTextureUpdateDesc desc = {};
        const void* data = nullptr;
        uint64 data_size = 0U;
    };
}
