#pragma once
#include "opengl_resource.h"
#include "opengl_state.h"
#include "tbx/types/assets/shader.h"
#include "tbx/utils/result.h"
#include <glad/glad.h>

namespace opengl_rendering
{
    class OpenGlShader;

    tbx::Result create_shaders(
        const std::vector<tbx::Shader>& shader_desc,
        std::vector<std::shared_ptr<OpenGlShader>>& out_shaders);

    class OpenGlShader final : public IOpenGlResource
    {
      public:
        OpenGlShader(const tbx::Shader& shader);
        OpenGlShader(const OpenGlShader&) = delete;
        OpenGlShader& operator=(const OpenGlShader&) = delete;
        OpenGlShader(OpenGlShader&& other) noexcept;
        OpenGlShader& operator=(OpenGlShader&& other) noexcept;
        ~OpenGlShader() noexcept override;

        tbx::ShaderType get_type() const;
        bool compile();
        bool is_compiled() const;
        const std::string& get_last_error() const;

        void bind() override;
        void unbind() override;

        uint32 get_shader_id() const;

      private:
        std::string _source = {};
        std::string _last_error = {};
        uint32 _shader_id = 0;
        tbx::ShaderType _type = tbx::ShaderType::NONE;
    };

    class OpenGlShaderProgram final : public IOpenGlResource
    {
      public:
        OpenGlShaderProgram(const std::vector<std::shared_ptr<OpenGlShader>>& shaders);
        OpenGlShaderProgram(const OpenGlShaderProgram&) = delete;
        OpenGlShaderProgram& operator=(const OpenGlShaderProgram&) = delete;
        OpenGlShaderProgram(OpenGlShaderProgram&& other) noexcept;
        OpenGlShaderProgram& operator=(OpenGlShaderProgram&& other) noexcept;
        ~OpenGlShaderProgram() noexcept override;

        void bind() override;
        void unbind() override;

        uint32 get_program_id() const;
        const std::string& get_last_error() const;

      private:
        std::string _last_error = {};
        uint32 _program_id = 0;
    };

    /// @brief
    /// Purpose: Stores one backend-ready vertex buffer binding from a raster pipeline.
    struct OpenGlVertexBufferBinding
    {
        uint32 slot = 0U;
        GLsizei stride = 0;
    };

    /// @brief
    /// Purpose: Stores one compute pipeline's OpenGL resources.
    struct OpenGlComputePipelineResource
    {
        OpenGlShaderProgram program;
    };

    /// @brief
    /// Purpose: Stores one raster pipeline's OpenGL resources and state.
    struct OpenGlRasterPipelineResource
    {
        OpenGlShaderProgram program;
        GLuint vertex_array = 0U;
        OpenGlPipelineState state = {};
        GLenum primitive_type = GL_TRIANGLES;
        std::vector<OpenGlVertexBufferBinding> vertex_buffers = {};
    };
}
