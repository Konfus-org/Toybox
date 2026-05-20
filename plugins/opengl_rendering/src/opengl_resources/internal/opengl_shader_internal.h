#pragma once
#include "opengl_shader.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/typedefs.h"
#include <glad/glad.h>
#include <string>
#include <utility>

namespace opengl_rendering::internal
{
    static uint32 take_gl_handle(uint32& id) noexcept
    {
        return std::exchange(id, 0);
    }

    static GLenum to_gl_shader_type(tbx::ShaderType type)
    {
        switch (type)
        {
            case tbx::ShaderType::VERTEX:
                return GL_VERTEX_SHADER;
            case tbx::ShaderType::TESSELATION:
                return GL_TESS_EVALUATION_SHADER;
            case tbx::ShaderType::GEOMETRY:
                return GL_GEOMETRY_SHADER;
            case tbx::ShaderType::FRAGMENT:
                return GL_FRAGMENT_SHADER;
            case tbx::ShaderType::COMPUTE:
                return GL_COMPUTE_SHADER;
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported shader type.");
                return GL_VERTEX_SHADER;
        }
    }

    static void handle_shader_compile_error(uint32 shader_id, tbx::ShaderType type)
    {
        GLint length = 0;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &length);
        std::string error_log(static_cast<uint64>(length), '\0');
        glGetShaderInfoLog(shader_id, length, &length, error_log.data());
        TBX_TRACE_ERROR(
            "OpenGL rendering: shader compilation failure (type {}). {}",
            static_cast<int>(type),
            error_log);
    }

    static void handle_program_link_error(uint32 program_id)
    {
        GLint length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &length);
        std::string error_log(static_cast<uint64>(length), '\0');
        glGetProgramInfoLog(program_id, length, &length, error_log.data());
        TBX_TRACE_ERROR("OpenGL rendering: shader program link failure. {}", error_log);
    }

}
