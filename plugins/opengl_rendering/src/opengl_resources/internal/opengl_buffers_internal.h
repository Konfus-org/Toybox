#pragma once
#include "tbx/types/typedefs.h"
#include <utility>

namespace opengl_rendering::internal
{
    static uint32 take_gl_handle(uint32& id) noexcept
    {
        return std::exchange(id, 0U);
    }
}
