#pragma once
#include "opengl_resource.h"
#include "tbx/common/typedefs.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/shader.h"
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace opengl_rendering
{
    struct OpenGlMaterialParams
    {
        std::vector<tbx::MaterialParameter> parameters = {};
        std::vector<std::string> texture_names = {};
    };

    struct OpenGlMaterialBlockUniform
    {
        std::string name = {};
        uint32 type = 0;
        int offset = 0;
        int size = 0;
    };

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

        bool try_upload(const tbx::MaterialParameter& uniform);
        bool try_upload(const OpenGlMaterialParams& params);

        uint32 get_program_id() const;
        int get_instance_model_attribute_location() const;
        int get_instance_id_attribute_location() const;

      private:
        int get_cached_uniform_location(const std::string& name);

        uint32 _program_id = 0;

        std::unordered_map<std::string, int> _uniform_locations = {};
        std::vector<std::string> _sampler_uniform_layout = {};
        std::vector<OpenGlMaterialBlockUniform> _material_uniforms = {};
        std::vector<std::byte> _material_uniform_data = {};

        uint32 _material_uniform_buffer = 0;
        int _material_uniform_block_size = 0;
        bool _has_material_uniform_block = false;
        int _instance_model_attribute_location = 8;
        int _instance_id_attribute_location = 12;

        std::unordered_set<std::string> _logged_missing_uniforms = {};
    };
}
