#pragma once
#include "../opengl_shader.h"
#include "../opengl_utils.h"
#include "tbx/systems/debugging/macros.h"
#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace opengl_rendering::internal
{
    static std::string gl_error_to_string(const GLenum error)
    {
        switch (error)
        {
            case GL_INVALID_ENUM:
                return "GL_INVALID_ENUM";
            case GL_INVALID_VALUE:
                return "GL_INVALID_VALUE";
            case GL_INVALID_OPERATION:
                return "GL_INVALID_OPERATION";
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                return "GL_INVALID_FRAMEBUFFER_OPERATION";
            case GL_OUT_OF_MEMORY:
                return "GL_OUT_OF_MEMORY";
            case GL_STACK_UNDERFLOW:
                return "GL_STACK_UNDERFLOW";
            case GL_STACK_OVERFLOW:
                return "GL_STACK_OVERFLOW";
            default:
                return "0x" + std::to_string(static_cast<uint32>(error));
        }
    }

}
