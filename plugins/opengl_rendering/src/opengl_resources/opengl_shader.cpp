#include "opengl_shader.h"
#include "tbx/common/int.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace opengl_rendering
{
    static std::string normalize_uniform_name(const std::string& name)
    {
        if (name.find("u_") != 0)
            return "u_" + name;
        return name;
    }

    static tbx::uint32 take_gl_handle(tbx::uint32& id) noexcept
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

    static void handle_shader_compile_error(tbx::uint32 shader_id, tbx::ShaderType type)
    {
        GLint length = 0;
        glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &length);
        std::string error_log(static_cast<tbx::uint64>(length), '\0');
        glGetShaderInfoLog(shader_id, length, &length, error_log.data());
        TBX_ASSERT(
            false,
            "OpenGL rendering: shader compilation failure (type {}). {}",
            static_cast<int>(type),
            error_log);
    }

    static void handle_program_link_error(tbx::uint32 program_id)
    {
        GLint length = 0;
        glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &length);
        std::string error_log(static_cast<tbx::uint64>(length), '\0');
        glGetProgramInfoLog(program_id, length, &length, error_log.data());
        TBX_ASSERT(false, "OpenGL rendering: shader program link failure. {}", error_log);
    }

    static GLint uniform_location(tbx::uint32 program_id, const std::string& name)
    {
        return glGetUniformLocation(program_id, name.c_str());
    }

    static void upload_uniform_value(GLint location, const tbx::UniformData& data)
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
                else if constexpr (std::is_same_v<ValueType, double>)
                {
                    glUniform1f(location, static_cast<float>(value));
                }
                else if constexpr (std::is_same_v<ValueType, tbx::Vec2>)
                {
                    glUniform2f(location, value.x, value.y);
                }
                else if constexpr (std::is_same_v<ValueType, tbx::Vec3>)
                {
                    glUniform3f(location, value.x, value.y, value.z);
                }
                else if constexpr (std::is_same_v<ValueType, tbx::Vec4>)
                {
                    glUniform4f(location, value.x, value.y, value.z, value.w);
                }
                else if constexpr (std::is_same_v<ValueType, tbx::Color>)
                {
                    glUniform4f(location, value.r, value.g, value.b, value.a);
                }
                else if constexpr (std::is_same_v<ValueType, tbx::Mat3>)
                {
                    glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(value));
                }
                else if constexpr (std::is_same_v<ValueType, tbx::Mat4>)
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

    OpenGlShader::OpenGlShader(const tbx::ShaderSource& shader)
        : _type(shader.type)
    {
        TBX_ASSERT(
            shader.type != tbx::ShaderType::NONE,
            "OpenGL rendering: shader source type must be a concrete stage.");
        TBX_ASSERT(!shader.source.empty(), "OpenGL rendering: shader source must not be empty.");

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

        TBX_ASSERT(_shader_id != 0, "OpenGL rendering: failed to create a valid shader object.");
    }

    OpenGlShader::OpenGlShader(OpenGlShader&& other) noexcept
        : _shader_id(take_gl_handle(other._shader_id))
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

    void OpenGlShader::bind() {}

    void OpenGlShader::unbind() {}

    tbx::uint32 OpenGlShader::get_shader_id() const
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
        , _uniform_locations(std::move(other._uniform_locations))
        , _sampler_uniform_layout(std::move(other._sampler_uniform_layout))
        , _logged_missing_uniforms(std::move(other._logged_missing_uniforms))
    {
    }

    OpenGlShaderProgram& OpenGlShaderProgram::operator=(OpenGlShaderProgram&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_program_id != 0)
            glDeleteProgram(_program_id);

        _program_id = take_gl_handle(other._program_id);
        _uniform_locations = std::move(other._uniform_locations);
        _sampler_uniform_layout = std::move(other._sampler_uniform_layout);
        _logged_missing_uniforms = std::move(other._logged_missing_uniforms);
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

    bool OpenGlShaderProgram::try_upload(const tbx::MaterialParameter& uniform)
    {
        if (_program_id == 0)
            return false;

        const GLint location = get_cached_uniform_location(normalize_uniform_name(uniform.name));
        if (location < 0)
            return false;

        upload_uniform_value(location, uniform.data);
        return true;
    }

    bool OpenGlShaderProgram::try_upload(const OpenGlMaterialParams& params)
    {
        if (_program_id == 0)
            return false;

        for (const auto& parameter : params.parameters)
        {
            if (!try_upload(parameter))
                return false;
        }

        bool is_same_sampler_layout = _sampler_uniform_layout.size() == params.textures.size();
        if (is_same_sampler_layout)
        {
            for (std::size_t texture_slot = 0; texture_slot < params.textures.size(); ++texture_slot)
            {
                const auto normalized_name = normalize_uniform_name(params.textures[texture_slot].name);
                if (_sampler_uniform_layout[texture_slot] != normalized_name)
                {
                    is_same_sampler_layout = false;
                    break;
                }
            }
        }

        if (is_same_sampler_layout)
            return true;

        _sampler_uniform_layout.clear();
        _sampler_uniform_layout.reserve(params.textures.size());
        for (std::size_t texture_slot = 0; texture_slot < params.textures.size(); ++texture_slot)
        {
            const auto& texture_binding = params.textures[texture_slot];
            const auto normalized_name = normalize_uniform_name(texture_binding.name);
            const auto sampler_uniform = tbx::MaterialParameter {
                .name = normalized_name,
                .data = static_cast<int>(texture_slot),
            };
            if (!try_upload(sampler_uniform))
                return false;
            _sampler_uniform_layout.push_back(normalized_name);
        }

        return true;
    }

    tbx::uint32 OpenGlShaderProgram::get_program_id() const
    {
        return _program_id;
    }

    int OpenGlShaderProgram::get_cached_uniform_location(const std::string& name)
    {
        if (const auto it = _uniform_locations.find(name); it != _uniform_locations.end())
        {
            return it->second;
        }

        const GLint location = uniform_location(_program_id, name);
        _uniform_locations.emplace(name, static_cast<int>(location));
        return location;
    }
}
