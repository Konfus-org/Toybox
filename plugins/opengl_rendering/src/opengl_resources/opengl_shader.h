#pragma once
#include "opengl_resource.h"
#include "tbx/common/int.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/shader.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace opengl_rendering
{

    struct OpenGlMaterialTexture
    {
        std::string name = "";
        tbx::Uuid texture_id = {};
    };

    struct OpenGlMaterialParams
    {
        std::vector<tbx::MaterialParameter> parameters = {};
        std::vector<OpenGlMaterialTexture> textures = {};
    };

    class OpenGlShader final : public IOpenGlResource
    {
      public:
        explicit OpenGlShader(const tbx::ShaderSource& shader);
        OpenGlShader(const OpenGlShader&) = delete;
        OpenGlShader& operator=(const OpenGlShader&) = delete;
        OpenGlShader(OpenGlShader&& other) noexcept;
        OpenGlShader& operator=(OpenGlShader&& other) noexcept;
        ~OpenGlShader() noexcept override;

        tbx::ShaderType get_type() const;

        void bind() override;
        void unbind() override;

        tbx::uint32 get_shader_id() const;

      private:
        tbx::uint32 _shader_id = 0;
        tbx::ShaderType _type = tbx::ShaderType::NONE;
    };

    class OpenGlShaderProgram final : public IOpenGlResource
    {
      public:
        explicit OpenGlShaderProgram(const std::vector<std::shared_ptr<OpenGlShader>>& shaders);
        OpenGlShaderProgram(const OpenGlShaderProgram&) = delete;
        OpenGlShaderProgram& operator=(const OpenGlShaderProgram&) = delete;
        OpenGlShaderProgram(OpenGlShaderProgram&& other) noexcept;
        OpenGlShaderProgram& operator=(OpenGlShaderProgram&& other) noexcept;
        ~OpenGlShaderProgram() noexcept override;

        void bind() override;
        void unbind() override;

        bool try_upload(const tbx::MaterialParameter& uniform);
        bool try_upload(const OpenGlMaterialParams& params);

        tbx::uint32 get_program_id() const;

      private:
        int get_cached_uniform_location(const std::string& name);

        tbx::uint32 _program_id = 0;
        std::unordered_map<std::string, int> _uniform_locations = {};
        std::unordered_set<std::string> _logged_missing_uniforms = {};
    };
}
