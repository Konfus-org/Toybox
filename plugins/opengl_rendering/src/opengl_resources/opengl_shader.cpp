#include "opengl_shader.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/typedefs.h"
#include <glad/glad.h>
#include <string>
#include <utility>

namespace opengl_rendering
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

    OpenGlShader::OpenGlShader(const tbx::ShaderSource& shader)
        : _source(shader.source)
        , _type(shader.type)
    {
        TBX_ASSERT(
            shader.type != tbx::ShaderType::NONE,
            "OpenGL rendering: shader source type must be a concrete stage.");
        TBX_ASSERT(!shader.source.empty(), "OpenGL rendering: shader source must not be empty.");
    }

    OpenGlShader::OpenGlShader(OpenGlShader&& other) noexcept
        : _source(std::move(other._source))
        , _shader_id(take_gl_handle(other._shader_id))
        , _type(other._type)
    {
        other._type = tbx::ShaderType::NONE;
    }

    OpenGlShader& OpenGlShader::operator=(OpenGlShader&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_shader_id != 0)
            glDeleteShader(_shader_id);

        _source = std::move(other._source);
        _shader_id = take_gl_handle(other._shader_id);
        _type = other._type;
        other._type = tbx::ShaderType::NONE;
        return *this;
    }

    OpenGlShader::~OpenGlShader() noexcept
    {
        if (_shader_id != 0)
        {
            glDeleteShader(_shader_id);
        }
    }

    tbx::ShaderType OpenGlShader::get_type() const
    {
        return _type;
    }

    bool OpenGlShader::compile()
    {
        if (_shader_id != 0)
            return true;

        if (_type == tbx::ShaderType::NONE || _source.empty())
            return false;

        const auto gl_type = to_gl_shader_type(_type);
        _shader_id = glCreateShader(gl_type);
        if (_shader_id == 0)
            return false;

        const auto* source = _source.c_str();
        glShaderSource(_shader_id, 1, &source, nullptr);
        glCompileShader(_shader_id);

        GLint compiled = 0;
        glGetShaderiv(_shader_id, GL_COMPILE_STATUS, &compiled);
        if (compiled == GL_FALSE)
        {
            handle_shader_compile_error(_shader_id, _type);
            glDeleteShader(_shader_id);
            _shader_id = 0;
            return false;
        }

        return true;
    }

    bool OpenGlShader::is_compiled() const
    {
        return _shader_id != 0;
    }

    void OpenGlShader::bind() {}

    void OpenGlShader::unbind() {}

    uint32 OpenGlShader::get_shader_id() const
    {
        return _shader_id;
    }

    OpenGlShaderProgram::OpenGlShaderProgram(
        const std::vector<std::shared_ptr<OpenGlShader>>& shaders)
    {
        _program_id = glCreateProgram();
        TBX_ASSERT(_program_id != 0, "OpenGL rendering: failed to create shader program object.");

        for (const auto& shader : shaders)
        {
            if (!shader)
            {
                continue;
            }
            TBX_ASSERT(
                shader->get_shader_id() != 0,
                "OpenGL rendering: attempted to link an invalid shader stage.");
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

    OpenGlShaderProgram::OpenGlShaderProgram(OpenGlShaderProgram&& other) noexcept
        : _program_id(take_gl_handle(other._program_id))
    {
    }

    OpenGlShaderProgram& OpenGlShaderProgram::operator=(OpenGlShaderProgram&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_program_id != 0)
            glDeleteProgram(_program_id);

        _program_id = take_gl_handle(other._program_id);
        return *this;
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
        TBX_ASSERT(_program_id != 0, "OpenGL rendering: cannot bind an invalid shader program.");
        glUseProgram(_program_id);
    }

    void OpenGlShaderProgram::unbind()
    {
        glUseProgram(0);
    }

    uint32 OpenGlShaderProgram::get_program_id() const
    {
        return _program_id;
    }
}
