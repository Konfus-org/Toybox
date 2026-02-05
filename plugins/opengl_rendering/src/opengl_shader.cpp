#include "opengl_shader.h"
#include "tbx/common/int.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <type_traits>
#include <variant>

namespace tbx::plugins
{
    static GLenum to_gl_shader_type(ShaderType type)
    {
        switch (type)
        {
            case ShaderType::VERTEX:
                return GL_VERTEX_SHADER;
            case ShaderType::FRAGMENT:
                return GL_FRAGMENT_SHADER;
            case ShaderType::COMPUTE:
                return GL_COMPUTE_SHADER;
            default:
                TBX_ASSERT(false, "OpenGL rendering: unsupported shader type.");
                return GL_VERTEX_SHADER;
        }
    }

    static void handle_shader_compile_error(uint32 shader_id, ShaderType type)
    {
        GLint length = 0;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &length);
        std::string error_log(static_cast<uint64>(length), '\0');
        glGetShaderInfoLog(shader_id, length, &length, error_log.data());
        TBX_ASSERT(
            false,
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
        TBX_ASSERT(
            false,
            "OpenGL rendering: shader program link failure. {}",
            error_log);
    }

    static GLint uniform_location(uint32 program_id, const std::string& name)
    {
        return glGetUniformLocation(program_id, name.c_str());
    }

    static void upload_uniform_value(GLint location, const UniformData& data)
    {
        std::visit(
            [location](const auto& value)
            {
                using ValueType = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<ValueType, bool>)
                {
                    glUniform1i(location, value ? 1 : 0);
                }
                else if constexpr (std::is_same_v<ValueType, int>)
                {
                    glUniform1i(location, value);
                }
                else if constexpr (std::is_same_v<ValueType, float>)
                {
                    glUniform1f(location, value);
                }
                else if constexpr (std::is_same_v<ValueType, Vec2>)
                {
                    glUniform2f(location, value.x, value.y);
                }
                else if constexpr (std::is_same_v<ValueType, Vec3>)
                {
                    glUniform3f(location, value.x, value.y, value.z);
                }
                else if constexpr (std::is_same_v<ValueType, Vec4>)
                {
                    glUniform4f(location, value.x, value.y, value.z, value.w);
                }
                else if constexpr (std::is_same_v<ValueType, RgbaColor>)
                {
                    glUniform4f(location, value.r, value.g, value.b, value.a);
                }
                else if constexpr (std::is_same_v<ValueType, Mat4>)
                {
                    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
                }
                else
                {
                    TBX_ASSERT(false, "OpenGL rendering: unsupported uniform type.");
                }
            },
            data);
    }

    OpenGlShader::OpenGlShader(const ShaderSource& shader)
        : _type(shader.type)
    {
        const auto gl_type = to_gl_shader_type(shader.type);
        _shader_id = glCreateShader(gl_type);

        const auto* source = shader.source.c_str();
        glShaderSource(_shader_id, 1, &source, nullptr);
        glCompileShader(_shader_id);

        GLint compiled = 0;
        glGetShaderiv(_shader_id, GL_COMPILE_STATUS, &compiled);
        if (compiled == GL_FALSE)
        {
            handle_shader_compile_error(_shader_id, shader.type);
            glDeleteShader(_shader_id);
            _shader_id = 0;
        }
    }

    OpenGlShader::~OpenGlShader() noexcept
    {
        if (_shader_id != 0)
        {
            glDeleteShader(_shader_id);
        }
    }

    ShaderType OpenGlShader::get_type() const
    {
        return _type;
    }

    void OpenGlShader::bind()
    {
    }

    void OpenGlShader::unbind()
    {
    }

    uint32 OpenGlShader::get_shader_id() const
    {
        return _shader_id;
    }

    OpenGlShaderProgram::OpenGlShaderProgram(
        const std::vector<std::shared_ptr<OpenGlShader>>& shaders)
    {
        _program_id = glCreateProgram();

        for (const auto& shader : shaders)
        {
            if (!shader)
            {
                continue;
            }
            glAttachShader(_program_id, shader->get_shader_id());
        }

        glLinkProgram(_program_id);

        GLint linked = 0;
        glGetProgramiv(_program_id, GL_LINK_STATUS, &linked);
        if (linked == GL_FALSE)
        {
            handle_program_link_error(_program_id);
            glDeleteProgram(_program_id);
            _program_id = 0;
            return;
        }

        for (const auto& shader : shaders)
        {
            if (shader)
            {
                glDetachShader(_program_id, shader->get_shader_id());
            }
        }
    }

    OpenGlShaderProgram::~OpenGlShaderProgram() noexcept
    {
        if (_program_id != 0)
        {
            glDeleteProgram(_program_id);
        }
    }

    void OpenGlShaderProgram::bind()
    {
        glUseProgram(_program_id);
    }

    void OpenGlShaderProgram::unbind()
    {
        glUseProgram(0);
    }

    void OpenGlShaderProgram::upload(const ShaderUniform& uniform)
    {
        if (try_upload(uniform))
        {
            return;
        }

        if (_logged_missing_uniforms.insert(uniform.name).second)
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: uniform '{}' not found on program {}.",
                uniform.name,
                _program_id);
        }
    }

    bool OpenGlShaderProgram::try_upload(const ShaderUniform& uniform)
    {
        if (_program_id == 0)
        {
            return false;
        }

        const GLint location = static_cast<GLint>(get_cached_uniform_location(uniform.name));
        if (location < 0)
        {
            return false;
        }

        upload_uniform_value(location, uniform.data);
        return true;
    }

    uint32 OpenGlShaderProgram::get_program_id() const
    {
        return _program_id;
    }

    int OpenGlShaderProgram::get_cached_uniform_location(const std::string& name)
    {
        const auto it = _uniform_locations.find(name);
        if (it != _uniform_locations.end())
        {
            return it->second;
        }

        const GLint location = uniform_location(_program_id, name);
        _uniform_locations.emplace(name, static_cast<int>(location));
        return static_cast<int>(location);
    }
}
