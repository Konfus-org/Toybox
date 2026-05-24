#pragma once
#include "../opengl_sampler.h"
#include <utility>

namespace opengl_rendering::internal
{
    static GLuint take_gl_handle(GLuint& handle) noexcept
    {
        return std::exchange(handle, 0U);
    }

}
