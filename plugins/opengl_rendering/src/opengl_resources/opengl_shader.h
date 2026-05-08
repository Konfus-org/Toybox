#pragma once
#include "opengl_resource.h"
#include "tbx/types/shader.h"
#include "tbx/types/typedefs.h"
#include <memory>
#include <string>
#include <vector>

namespace opengl_rendering
{
    class OpenGlShader final : public IOpenGlResource
    {
      public:
        OpenGlShader(const tbx::ShaderSource& shader);
        OpenGlShader(const OpenGlShader&) = delete;
        OpenGlShader& operator=(const OpenGlShader&) = delete;
        OpenGlShader(OpenGlShader&& other) noexcept;
        OpenGlShader& operator=(OpenGlShader&& other) noexcept;
        ~OpenGlShader() noexcept override;

        tbx::ShaderType get_type() const;
        bool compile();
        bool is_compiled() const;

        void bind() override;
        void unbind() override;

        uint32 get_shader_id() const;

      private:
        std::string _source = {};
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
        int get_instance_model_attribute_location() const;
        int get_instance_id_attribute_location() const;

      private:
        uint32 _program_id = 0;

        int _instance_model_attribute_location = 8;
        int _instance_id_attribute_location = 12;
    };
}
