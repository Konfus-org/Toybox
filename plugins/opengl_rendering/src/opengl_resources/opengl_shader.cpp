#include "opengl_shader.h"
#include "opengl_bindless.h"
#include "tbx/common/typedefs.h"
#include "tbx/debugging/macros.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace opengl_rendering
{
    static constexpr auto MATERIAL_SURFACE_UBO_BINDING = uint32 {9U};

    static std::string normalize_uniform_name(const std::string& name)
    {
        if (name.find("u_") != 0)
            return "u_" + name;
        return name;
    }

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
        TBX_TRACE_WARNING(
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
        TBX_TRACE_WARNING("OpenGL rendering: shader program link failure. {}", error_log);
    }

    static GLint uniform_location(uint32 program_id, const std::string& name)
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

    static bool is_matching_parameter_name(
        const std::string& candidate_name,
        const std::string_view requested_name)
    {
        if (candidate_name == requested_name)
            return true;
        if (candidate_name.size() > 2U && candidate_name[0] == 'u' && candidate_name[1] == '_')
            return std::string_view(candidate_name).substr(2U) == requested_name;
        return false;
    }

    static const tbx::MaterialParameter* try_find_material_parameter(
        const OpenGlMaterialParams& params,
        const std::string_view parameter_name)
    {
        for (const auto& parameter : params.parameters)
            if (is_matching_parameter_name(parameter.name, parameter_name))
                return &parameter;

        return nullptr;
    }

    static std::string get_material_block_parameter_name(const std::string& uniform_name)
    {
        auto parameter_name = uniform_name;
        if (const auto member_separator_index = parameter_name.rfind('.');
            member_separator_index != std::string::npos)
            parameter_name = parameter_name.substr(member_separator_index + 1U);
        if (parameter_name.rfind("u_", 0U) == 0U)
            parameter_name = parameter_name.substr(2U);
        return parameter_name;
    }

    static bool is_material_block_parameter(
        const std::string& parameter_name,
        const std::vector<OpenGlMaterialBlockUniform>& block_uniforms)
    {
        for (const auto& block_uniform : block_uniforms)
            if (is_matching_parameter_name(parameter_name, block_uniform.name))
                return true;

        return false;
    }

    static bool try_get_parameter_float_value(
        const tbx::MaterialParameterData& data,
        float& out_value)
    {
        if (const auto* value = std::get_if<float>(&data))
        {
            out_value = *value;
            return true;
        }
        if (const auto* value = std::get_if<double>(&data))
        {
            out_value = static_cast<float>(*value);
            return true;
        }
        if (const auto* value = std::get_if<int>(&data))
        {
            out_value = static_cast<float>(*value);
            return true;
        }

        return false;
    }

    static bool try_get_parameter_vec2_value(
        const tbx::MaterialParameterData& data,
        tbx::Vec2& out_value)
    {
        if (const auto* value = std::get_if<tbx::Vec2>(&data))
        {
            out_value = *value;
            return true;
        }

        return false;
    }

    static bool try_get_parameter_vec3_value(
        const tbx::MaterialParameterData& data,
        tbx::Vec3& out_value)
    {
        if (const auto* value = std::get_if<tbx::Vec3>(&data))
        {
            out_value = *value;
            return true;
        }

        return false;
    }

    static bool try_get_parameter_vec4_value(
        const tbx::MaterialParameterData& data,
        tbx::Vec4& out_value)
    {
        if (const auto* value = std::get_if<tbx::Vec4>(&data))
        {
            out_value = *value;
            return true;
        }
        if (const auto* value = std::get_if<tbx::Color>(&data))
        {
            out_value = tbx::Vec4(value->r, value->g, value->b, value->a);
            return true;
        }

        return false;
    }

    static bool try_write_material_block_uniform(
        std::byte* block_data,
        const int block_size,
        const OpenGlMaterialBlockUniform& block_uniform,
        const OpenGlMaterialParams& params)
    {
        if (block_data == nullptr || block_uniform.offset < 0 || block_uniform.offset >= block_size)
            return false;
        if (block_uniform.size != 1)
            return false;

        const auto* parameter = try_find_material_parameter(params, block_uniform.name);
        if (parameter == nullptr)
            return false;

        auto* destination = block_data + block_uniform.offset;
        switch (block_uniform.type)
        {
            case GL_FLOAT:
            {
                auto value = 0.0F;
                if (!try_get_parameter_float_value(parameter->data, value))
                    return false;
                if (block_uniform.offset + static_cast<int>(sizeof(value)) > block_size)
                    return false;
                std::memcpy(destination, &value, sizeof(value));
                return true;
            }
            case GL_FLOAT_VEC2:
            {
                auto value = tbx::Vec2(0.0F);
                if (!try_get_parameter_vec2_value(parameter->data, value))
                    return false;
                if (block_uniform.offset + static_cast<int>(sizeof(value)) > block_size)
                    return false;
                std::memcpy(destination, &value, sizeof(value));
                return true;
            }
            case GL_FLOAT_VEC3:
            {
                auto value = tbx::Vec3(0.0F);
                if (!try_get_parameter_vec3_value(parameter->data, value))
                    return false;
                if (block_uniform.offset + static_cast<int>(sizeof(value)) > block_size)
                    return false;
                std::memcpy(destination, &value, sizeof(value));
                return true;
            }
            case GL_FLOAT_VEC4:
            {
                auto value = tbx::Vec4(0.0F);
                if (!try_get_parameter_vec4_value(parameter->data, value))
                    return false;
                if (block_uniform.offset + static_cast<int>(sizeof(value)) > block_size)
                    return false;
                std::memcpy(destination, &value, sizeof(value));
                return true;
            }
            default:
                return false;
        }
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

        const auto material_surface_block_index =
            glGetUniformBlockIndex(_program_id, "MaterialSurfaceBlock");
        if (material_surface_block_index != GL_INVALID_INDEX)
        {
            _has_material_uniform_block = true;
            auto block_size = GLint {0};
            glGetActiveUniformBlockiv(
                _program_id,
                material_surface_block_index,
                GL_UNIFORM_BLOCK_DATA_SIZE,
                &block_size);
            _material_uniform_block_size =
                block_size > 0 ? block_size : static_cast<int>(sizeof(tbx::Vec4) * 3U);
            auto active_uniform_count = GLint {0};
            glGetActiveUniformBlockiv(
                _program_id,
                material_surface_block_index,
                GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS,
                &active_uniform_count);
            if (active_uniform_count > 0)
            {
                auto active_uniform_indices =
                    std::vector<GLint>(static_cast<std::size_t>(active_uniform_count), 0);
                glGetActiveUniformBlockiv(
                    _program_id,
                    material_surface_block_index,
                    GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,
                    active_uniform_indices.data());

                auto max_uniform_name_length = GLint {0};
                glGetProgramiv(_program_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_uniform_name_length);
                const auto uniform_name_buffer_size =
                    max_uniform_name_length > 0 ? max_uniform_name_length : 256;

                _material_uniforms.reserve(static_cast<std::size_t>(active_uniform_count));
                for (const auto uniform_index_value : active_uniform_indices)
                {
                    const auto uniform_index = static_cast<GLuint>(uniform_index_value);
                    auto uniform_type = GLint {0};
                    auto uniform_offset = GLint {0};
                    auto uniform_size = GLint {0};
                    glGetActiveUniformsiv(
                        _program_id,
                        1,
                        &uniform_index,
                        GL_UNIFORM_TYPE,
                        &uniform_type);
                    glGetActiveUniformsiv(
                        _program_id,
                        1,
                        &uniform_index,
                        GL_UNIFORM_OFFSET,
                        &uniform_offset);
                    glGetActiveUniformsiv(
                        _program_id,
                        1,
                        &uniform_index,
                        GL_UNIFORM_SIZE,
                        &uniform_size);

                    auto uniform_name =
                        std::string(static_cast<std::size_t>(uniform_name_buffer_size), '\0');
                    auto written_name_length = GLsizei {0};
                    glGetActiveUniformName(
                        _program_id,
                        uniform_index,
                        uniform_name_buffer_size,
                        &written_name_length,
                        uniform_name.data());
                    uniform_name.resize(static_cast<std::size_t>(written_name_length));

                    _material_uniforms.push_back(
                        OpenGlMaterialBlockUniform {
                            .name = get_material_block_parameter_name(uniform_name),
                            .type = static_cast<uint32>(uniform_type),
                            .offset = uniform_offset,
                            .size = uniform_size,
                        });
                }
                std::sort(
                    _material_uniforms.begin(),
                    _material_uniforms.end(),
                    [](const OpenGlMaterialBlockUniform& left,
                       const OpenGlMaterialBlockUniform& right)
                    {
                        return left.offset < right.offset;
                    });
            }
            glUniformBlockBinding(
                _program_id,
                material_surface_block_index,
                MATERIAL_SURFACE_UBO_BINDING);
        }

        _instance_model_attribute_location = 8;
        _instance_id_attribute_location = 12;
    }

    OpenGlShaderProgram::OpenGlShaderProgram(OpenGlShaderProgram&& other) noexcept
        : _program_id(take_gl_handle(other._program_id))
        , _uniform_locations(std::move(other._uniform_locations))
        , _sampler_uniform_layout(std::move(other._sampler_uniform_layout))
        , _material_uniforms(std::move(other._material_uniforms))
        , _material_uniform_data(std::move(other._material_uniform_data))
        , _material_uniform_buffer(take_gl_handle(other._material_uniform_buffer))
        , _material_uniform_block_size(other._material_uniform_block_size)
        , _has_material_uniform_block(other._has_material_uniform_block)
        , _instance_model_attribute_location(other._instance_model_attribute_location)
        , _instance_id_attribute_location(other._instance_id_attribute_location)
        , _logged_missing_uniforms(std::move(other._logged_missing_uniforms))
    {
        other._material_uniform_block_size = 0;
        other._has_material_uniform_block = false;
        other._instance_model_attribute_location = 8;
        other._instance_id_attribute_location = 12;
    }

    OpenGlShaderProgram& OpenGlShaderProgram::operator=(OpenGlShaderProgram&& other) noexcept
    {
        if (this == &other)
            return *this;

        if (_program_id != 0)
            glDeleteProgram(_program_id);
        if (_material_uniform_buffer != 0)
            glDeleteBuffers(1, &_material_uniform_buffer);

        _program_id = take_gl_handle(other._program_id);
        _uniform_locations = std::move(other._uniform_locations);
        _sampler_uniform_layout = std::move(other._sampler_uniform_layout);
        _logged_missing_uniforms = std::move(other._logged_missing_uniforms);
        _material_uniform_buffer = take_gl_handle(other._material_uniform_buffer);
        _material_uniform_block_size = other._material_uniform_block_size;
        _material_uniforms = std::move(other._material_uniforms);
        _material_uniform_data = std::move(other._material_uniform_data);
        _has_material_uniform_block = other._has_material_uniform_block;
        _instance_model_attribute_location = other._instance_model_attribute_location;
        _instance_id_attribute_location = other._instance_id_attribute_location;
        other._material_uniform_block_size = 0;
        other._has_material_uniform_block = false;
        other._instance_model_attribute_location = 8;
        other._instance_id_attribute_location = 12;
        return *this;
    }

    OpenGlShaderProgram::~OpenGlShaderProgram() noexcept
    {
        if (_program_id != 0)
        {
            glDeleteProgram(_program_id);
        }
        if (_material_uniform_buffer != 0)
        {
            glDeleteBuffers(1, &_material_uniform_buffer);
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

        auto used_material_uniform_block = false;
        if (_has_material_uniform_block)
        {
            if (_material_uniform_buffer == 0)
            {
                glCreateBuffers(1, &_material_uniform_buffer);
                glNamedBufferData(
                    _material_uniform_buffer,
                    static_cast<GLsizeiptr>(_material_uniform_block_size),
                    nullptr,
                    GL_DYNAMIC_DRAW);
            }

            _material_uniform_data.assign(
                static_cast<std::size_t>(_material_uniform_block_size),
                std::byte {0});
            for (const auto& material_uniform : _material_uniforms)
                try_write_material_block_uniform(
                    _material_uniform_data.data(),
                    _material_uniform_block_size,
                    material_uniform,
                    params);

            glNamedBufferSubData(
                _material_uniform_buffer,
                0,
                static_cast<GLsizeiptr>(_material_uniform_data.size()),
                _material_uniform_data.data());
            glBindBufferBase(
                GL_UNIFORM_BUFFER,
                MATERIAL_SURFACE_UBO_BINDING,
                _material_uniform_buffer);
            used_material_uniform_block = true;
        }

        for (const auto& parameter : params.parameters)
        {
            if (used_material_uniform_block
                && is_material_block_parameter(parameter.name, _material_uniforms))
                continue;
            if (!try_upload(parameter))
                continue;
        }

        bool is_same_sampler_layout = _sampler_uniform_layout.size() == params.textures.size();
        if (is_same_sampler_layout)
        {
            for (std::size_t texture_slot = 0; texture_slot < params.textures.size();
                 ++texture_slot)
            {
                const auto normalized_name =
                    normalize_uniform_name(params.textures[texture_slot].name);
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
            const auto sampler_uniform =
                tbx::MaterialParameter(normalized_name, static_cast<int>(texture_slot));
            if (!try_upload(sampler_uniform))
                continue;
            _sampler_uniform_layout.push_back(normalized_name);
        }

        return true;
    }

    uint32 OpenGlShaderProgram::get_program_id() const
    {
        return _program_id;
    }

    int OpenGlShaderProgram::get_instance_model_attribute_location() const
    {
        return _instance_model_attribute_location;
    }

    int OpenGlShaderProgram::get_instance_id_attribute_location() const
    {
        return _instance_id_attribute_location;
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
