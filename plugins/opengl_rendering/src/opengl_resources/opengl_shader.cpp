#include "opengl_shader.h"
#include "opengl_bindless.h"
#include "tbx/common/int.h"
#include "tbx/debugging/macros.h"
#include <cstring>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <type_traits>
#include <unordered_map>
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

    static bool try_get_float(const tbx::MaterialParameterData& data, float& value)
    {
        if (const auto* scalar = std::get_if<float>(&data))
        {
            value = *scalar;
            return true;
        }
        if (const auto* scalar = std::get_if<double>(&data))
        {
            value = static_cast<float>(*scalar);
            return true;
        }
        if (const auto* scalar = std::get_if<int>(&data))
        {
            value = static_cast<float>(*scalar);
            return true;
        }
        return false;
    }

    static bool try_get_int(const tbx::MaterialParameterData& data, int& value)
    {
        if (const auto* scalar = std::get_if<int>(&data))
        {
            value = *scalar;
            return true;
        }
        if (const auto* scalar = std::get_if<float>(&data))
        {
            value = static_cast<int>(*scalar);
            return true;
        }
        if (const auto* scalar = std::get_if<double>(&data))
        {
            value = static_cast<int>(*scalar);
            return true;
        }
        if (const auto* bool_value = std::get_if<bool>(&data))
        {
            value = *bool_value ? 1 : 0;
            return true;
        }
        return false;
    }

    static bool try_get_uint(const tbx::MaterialParameterData& data, tbx::uint32& value)
    {
        if (const auto* scalar = std::get_if<int>(&data))
        {
            value = static_cast<tbx::uint32>(*scalar);
            return true;
        }
        if (const auto* scalar = std::get_if<float>(&data))
        {
            value = static_cast<tbx::uint32>(*scalar);
            return true;
        }
        if (const auto* scalar = std::get_if<double>(&data))
        {
            value = static_cast<tbx::uint32>(*scalar);
            return true;
        }
        if (const auto* bool_value = std::get_if<bool>(&data))
        {
            value = *bool_value ? 1U : 0U;
            return true;
        }
        return false;
    }

    static bool try_get_bool(const tbx::MaterialParameterData& data, bool& value)
    {
        if (const auto* bool_value = std::get_if<bool>(&data))
        {
            value = *bool_value;
            return true;
        }
        if (const auto* scalar = std::get_if<int>(&data))
        {
            value = *scalar != 0;
            return true;
        }
        if (const auto* scalar = std::get_if<float>(&data))
        {
            value = *scalar != 0.0f;
            return true;
        }
        return false;
    }

    static bool try_get_vec2(const tbx::MaterialParameterData& data, tbx::Vec2& value)
    {
        if (const auto* vec2 = std::get_if<tbx::Vec2>(&data))
        {
            value = *vec2;
            return true;
        }
        if (const auto* vec3 = std::get_if<tbx::Vec3>(&data))
        {
            value = tbx::Vec2(vec3->x, vec3->y);
            return true;
        }
        if (const auto* vec4 = std::get_if<tbx::Vec4>(&data))
        {
            value = tbx::Vec2(vec4->x, vec4->y);
            return true;
        }
        if (const auto* color = std::get_if<tbx::Color>(&data))
        {
            value = tbx::Vec2(color->r, color->g);
            return true;
        }
        return false;
    }

    static bool try_get_vec3(const tbx::MaterialParameterData& data, tbx::Vec3& value)
    {
        if (const auto* vec3 = std::get_if<tbx::Vec3>(&data))
        {
            value = *vec3;
            return true;
        }
        if (const auto* vec4 = std::get_if<tbx::Vec4>(&data))
        {
            value = tbx::Vec3(vec4->x, vec4->y, vec4->z);
            return true;
        }
        if (const auto* color = std::get_if<tbx::Color>(&data))
        {
            value = tbx::Vec3(color->r, color->g, color->b);
            return true;
        }
        if (const auto* vec2 = std::get_if<tbx::Vec2>(&data))
        {
            value = tbx::Vec3(vec2->x, vec2->y, 0.0f);
            return true;
        }
        return false;
    }

    static bool try_get_vec4(const tbx::MaterialParameterData& data, tbx::Vec4& value)
    {
        if (const auto* color = std::get_if<tbx::Color>(&data))
        {
            value = tbx::Vec4(color->r, color->g, color->b, color->a);
            return true;
        }
        if (const auto* vec4 = std::get_if<tbx::Vec4>(&data))
        {
            value = *vec4;
            return true;
        }
        if (const auto* vec3 = std::get_if<tbx::Vec3>(&data))
        {
            value = tbx::Vec4(vec3->x, vec3->y, vec3->z, 0.0f);
            return true;
        }
        if (const auto* vec2 = std::get_if<tbx::Vec2>(&data))
        {
            value = tbx::Vec4(vec2->x, vec2->y, 0.0f, 0.0f);
            return true;
        }
        return false;
    }

    static std::string normalize_material_block_uniform_name(const std::string& reflected_name)
    {
        auto normalized = reflected_name;
        if (const auto separator = normalized.find('.'); separator != std::string::npos)
            normalized = normalized.substr(separator + 1);
        if (const auto array_marker = normalized.find('['); array_marker != std::string::npos)
            normalized = normalized.substr(0, array_marker);
        return normalize_uniform_name(normalized);
    }

    static bool write_bytes(
        std::vector<tbx::uint8>& block_data,
        int offset,
        const void* source,
        std::size_t byte_count)
    {
        if (offset < 0)
            return false;
        const auto start = static_cast<std::size_t>(offset);
        if (start + byte_count > block_data.size())
            return false;
        std::memcpy(block_data.data() + start, source, byte_count);
        return true;
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

        const auto material_block_index = glGetUniformBlockIndex(_program_id, "MaterialParams");
        if (material_block_index == GL_INVALID_INDEX)
        {
            TBX_ASSERT(
                false,
                "OpenGL rendering: shader program is missing required MaterialParams uniform "
                "block.");
            glDeleteProgram(_program_id);
            _program_id = 0;
            return;
        }

        constexpr tbx::uint32 material_block_binding = 1;
        glUniformBlockBinding(_program_id, material_block_index, material_block_binding);
        GLint material_block_size = 0;
        glGetActiveUniformBlockiv(
            _program_id,
            material_block_index,
            GL_UNIFORM_BLOCK_DATA_SIZE,
            &material_block_size);
        _material_uniform_block_size = material_block_size;

        GLint uniform_count = 0;
        glGetActiveUniformBlockiv(
            _program_id,
            material_block_index,
            GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS,
            &uniform_count);

        _material_uniforms.clear();
        if (uniform_count > 0)
        {
            auto uniform_indices = std::vector<GLuint>(static_cast<std::size_t>(uniform_count), 0U);
            glGetActiveUniformBlockiv(
                _program_id,
                material_block_index,
                GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES,
                reinterpret_cast<GLint*>(uniform_indices.data()));

            auto uniform_offsets = std::vector<GLint>(static_cast<std::size_t>(uniform_count), 0);
            auto uniform_types = std::vector<GLint>(static_cast<std::size_t>(uniform_count), 0);
            auto uniform_sizes = std::vector<GLint>(static_cast<std::size_t>(uniform_count), 0);

            glGetActiveUniformsiv(
                _program_id,
                uniform_count,
                uniform_indices.data(),
                GL_UNIFORM_OFFSET,
                uniform_offsets.data());
            glGetActiveUniformsiv(
                _program_id,
                uniform_count,
                uniform_indices.data(),
                GL_UNIFORM_TYPE,
                uniform_types.data());
            glGetActiveUniformsiv(
                _program_id,
                uniform_count,
                uniform_indices.data(),
                GL_UNIFORM_SIZE,
                uniform_sizes.data());

            _material_uniforms.reserve(static_cast<std::size_t>(uniform_count));
            for (GLint i = 0; i < uniform_count; ++i)
            {
                GLsizei name_length = 0;
                GLchar name_buffer[256] = {};
                glGetActiveUniformName(
                    _program_id,
                    uniform_indices[static_cast<std::size_t>(i)],
                    static_cast<GLsizei>(sizeof(name_buffer)),
                    &name_length,
                    name_buffer);
                auto reflected_name =
                    std::string(name_buffer, static_cast<std::size_t>(name_length));
                _material_uniforms.push_back(
                    MaterialBlockUniform {
                        .name = normalize_material_block_uniform_name(reflected_name),
                        .type =
                            static_cast<tbx::uint32>(uniform_types[static_cast<std::size_t>(i)]),
                        .offset = uniform_offsets[static_cast<std::size_t>(i)],
                        .size = uniform_sizes[static_cast<std::size_t>(i)],
                    });
            }
        }

        glGenBuffers(1, &_material_uniform_buffer);
        glBindBuffer(GL_UNIFORM_BUFFER, _material_uniform_buffer);
        glBufferData(
            GL_UNIFORM_BUFFER,
            static_cast<GLsizeiptr>(_material_uniform_block_size),
            nullptr,
            GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_UNIFORM_BUFFER, material_block_binding, _material_uniform_buffer);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        _has_material_uniform_block = true;

        _instance_model_attribute_location = 8;
        _instance_id_attribute_location = 12;
    }

    OpenGlShaderProgram::OpenGlShaderProgram(OpenGlShaderProgram&& other) noexcept
        : _program_id(take_gl_handle(other._program_id))
        , _uniform_locations(std::move(other._uniform_locations))
        , _bindless_sampler_layout(std::move(other._bindless_sampler_layout))
        , _sampler_uniform_layout(std::move(other._sampler_uniform_layout))
        , _logged_missing_uniforms(std::move(other._logged_missing_uniforms))
        , _material_uniform_buffer(take_gl_handle(other._material_uniform_buffer))
        , _material_uniform_block_size(other._material_uniform_block_size)
        , _material_uniforms(std::move(other._material_uniforms))
        , _has_material_uniform_block(other._has_material_uniform_block)
        , _instance_model_attribute_location(other._instance_model_attribute_location)
        , _instance_id_attribute_location(other._instance_id_attribute_location)
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

        _program_id = take_gl_handle(other._program_id);
        _uniform_locations = std::move(other._uniform_locations);
        _bindless_sampler_layout = std::move(other._bindless_sampler_layout);
        _sampler_uniform_layout = std::move(other._sampler_uniform_layout);
        _logged_missing_uniforms = std::move(other._logged_missing_uniforms);
        if (_material_uniform_buffer != 0)
            glDeleteBuffers(1, &_material_uniform_buffer);
        _material_uniform_buffer = take_gl_handle(other._material_uniform_buffer);
        _material_uniform_block_size = other._material_uniform_block_size;
        _material_uniforms = std::move(other._material_uniforms);
        _has_material_uniform_block = other._has_material_uniform_block;
        other._material_uniform_block_size = 0;
        other._has_material_uniform_block = false;
        _instance_model_attribute_location = other._instance_model_attribute_location;
        _instance_id_attribute_location = other._instance_id_attribute_location;
        other._instance_model_attribute_location = 8;
        other._instance_id_attribute_location = 12;
        return *this;
    }

    OpenGlShaderProgram::~OpenGlShaderProgram() noexcept
    {
        if (_material_uniform_buffer != 0)
            glDeleteBuffers(1, &_material_uniform_buffer);

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
            const auto normalized_name = normalize_uniform_name(parameter.name);
            bool is_block_uniform = false;
            for (const auto& uniform : _material_uniforms)
            {
                if (uniform.name == normalized_name)
                {
                    is_block_uniform = true;
                    break;
                }
            }
            if (is_block_uniform)
                continue;

            const GLint location = get_cached_uniform_location(normalized_name);
            if (location < 0)
            {
                TBX_ASSERT(
                    false,
                    "OpenGL rendering: material parameter '{}' is not present on program {}.",
                    normalized_name,
                    _program_id);
                return false;
            }

            upload_uniform_value(location, parameter.data);
        }

        auto use_bindless_textures = !params.textures.empty();
        for (const auto& texture_binding : params.textures)
        {
            if (texture_binding.bindless_handle != 0)
                continue;

            use_bindless_textures = false;
            break;
        }

        for (std::size_t texture_index = 0; texture_index < params.textures.size(); ++texture_index)
        {
            const auto& texture_binding = params.textures[texture_index];
            const auto normalized_name = normalize_uniform_name(texture_binding.name);
            const GLint location = get_cached_uniform_location(normalized_name);
            if (location < 0)
            {
                TBX_ASSERT(
                    false,
                    "OpenGL rendering: sampler '{}' is not present on program {}.",
                    normalized_name,
                    _program_id);
                return false;
            }

            if (!use_bindless_textures)
            {
                glUniform1i(location, static_cast<GLint>(texture_index));
                continue;
            }

            if (const auto existing_sampler = _bindless_sampler_layout.find(normalized_name);
                existing_sampler != _bindless_sampler_layout.end()
                && existing_sampler->second == texture_binding.bindless_handle)
            {
                continue;
            }

            if (!try_upload_bindless_sampler(
                    _program_id,
                    location,
                    texture_binding.bindless_handle))
            {
                TBX_ASSERT(
                    false,
                    "OpenGL rendering: failed to upload bindless sampler '{}' on program {}.",
                    normalized_name,
                    _program_id);
                return false;
            }

            _bindless_sampler_layout.insert_or_assign(
                normalized_name,
                texture_binding.bindless_handle);
        }

        _sampler_uniform_layout.clear();
        return true;
    }

    bool OpenGlShaderProgram::try_upload_material_block(const OpenGlMaterialParams& params) const
    {
        if (_material_uniform_buffer == 0 || _material_uniform_block_size <= 0)
            return false;

        auto block_data =
            std::vector<tbx::uint8>(static_cast<std::size_t>(_material_uniform_block_size), 0U);

        auto parameter_lookup =
            std::unordered_map<std::string, const tbx::MaterialParameterData*>();
        parameter_lookup.reserve(params.parameters.size());
        for (const auto& parameter : params.parameters)
        {
            parameter_lookup.insert_or_assign(
                normalize_uniform_name(parameter.name),
                &parameter.data);
        }

        for (const auto& uniform : _material_uniforms)
        {
            const auto found_parameter = parameter_lookup.find(uniform.name);
            if (found_parameter == parameter_lookup.end() || !found_parameter->second)
            {
                TBX_ASSERT(
                    false,
                    "OpenGL rendering: material block uniform '{}' is missing on program {}.",
                    uniform.name,
                    _program_id);
                return false;
            }

            const auto& data = *found_parameter->second;
            switch (uniform.type)
            {
                case GL_BOOL:
                {
                    bool value = false;
                    if (!try_get_bool(data, value))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed to convert '{}' to bool for program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    const int packed_value = value ? 1 : 0;
                    if (!write_bytes(
                            block_data,
                            uniform.offset,
                            &packed_value,
                            sizeof(packed_value)))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed writing block bytes for '{}' on program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    break;
                }
                case GL_INT:
                {
                    int value = 0;
                    if (!try_get_int(data, value))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed to convert '{}' to int for program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    if (!write_bytes(block_data, uniform.offset, &value, sizeof(value)))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed writing block bytes for '{}' on program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    break;
                }
                case GL_UNSIGNED_INT:
                {
                    tbx::uint32 value = 0;
                    if (!try_get_uint(data, value))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed to convert '{}' to uint for program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    if (!write_bytes(block_data, uniform.offset, &value, sizeof(value)))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed writing block bytes for '{}' on program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    break;
                }
                case GL_FLOAT:
                {
                    float value = 0.0f;
                    if (!try_get_float(data, value))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed to convert '{}' to float for program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    if (!write_bytes(block_data, uniform.offset, &value, sizeof(value)))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed writing block bytes for '{}' on program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    break;
                }
                case GL_FLOAT_VEC2:
                {
                    auto value = tbx::Vec2(0.0f, 0.0f);
                    if (!try_get_vec2(data, value))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed to convert '{}' to vec2 for program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    if (!write_bytes(block_data, uniform.offset, &value[0], sizeof(float) * 2U))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed writing block bytes for '{}' on program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    break;
                }
                case GL_FLOAT_VEC3:
                {
                    auto value = tbx::Vec3(0.0f, 0.0f, 0.0f);
                    if (!try_get_vec3(data, value))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed to convert '{}' to vec3 for program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    if (!write_bytes(block_data, uniform.offset, &value[0], sizeof(float) * 3U))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed writing block bytes for '{}' on program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    break;
                }
                case GL_FLOAT_VEC4:
                {
                    auto value = tbx::Vec4(0.0f, 0.0f, 0.0f, 0.0f);
                    if (!try_get_vec4(data, value))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed to convert '{}' to vec4 for program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    if (!write_bytes(block_data, uniform.offset, &value[0], sizeof(float) * 4U))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed writing block bytes for '{}' on program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    break;
                }
                case GL_FLOAT_MAT3:
                {
                    const auto* value = std::get_if<tbx::Mat3>(&data);
                    if (value == nullptr)
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed to convert '{}' to mat3 for program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    if (!write_bytes(
                            block_data,
                            uniform.offset,
                            glm::value_ptr(*value),
                            sizeof(float) * 9U))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed writing block bytes for '{}' on program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    break;
                }
                case GL_FLOAT_MAT4:
                {
                    const auto* value = std::get_if<tbx::Mat4>(&data);
                    if (value == nullptr)
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed to convert '{}' to mat4 for program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    if (!write_bytes(
                            block_data,
                            uniform.offset,
                            glm::value_ptr(*value),
                            sizeof(float) * 16U))
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: failed writing block bytes for '{}' on program {}.",
                            uniform.name,
                            _program_id);
                        return false;
                    }
                    break;
                }
                default:
                    TBX_ASSERT(
                        false,
                        "OpenGL rendering: unsupported material block uniform type '{}' for '{}' "
                        "on program {}.",
                        uniform.type,
                        uniform.name,
                        _program_id);
                    return false;
            }
        }

        glBindBuffer(GL_UNIFORM_BUFFER, _material_uniform_buffer);
        glBufferSubData(
            GL_UNIFORM_BUFFER,
            0,
            static_cast<GLsizeiptr>(block_data.size()),
            block_data.data());
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        return true;
    }

    tbx::uint32 OpenGlShaderProgram::get_program_id() const
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
