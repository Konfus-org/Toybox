#include "opengl_fallbacks.h"
#include "opengl_uploader.h"
#include "tbx/debugging/macros.h"

namespace opengl_rendering
{
    const tbx::Uuid& get_fallback_texture_resource_id()
    {
        static const auto FALLBACK_TEXTURE_RESOURCE_ID = tbx::Uuid(0x7EAE00FFU);
        return FALLBACK_TEXTURE_RESOURCE_ID;
    }

    const tbx::Uuid& get_flat_normal_texture_resource_id()
    {
        static const auto FLAT_NORMAL_TEXTURE_RESOURCE_ID = tbx::Uuid(0x7EAE0101U);
        return FLAT_NORMAL_TEXTURE_RESOURCE_ID;
    }

    const tbx::Uuid& get_fallback_material_resource_id()
    {
        static const auto FALLBACK_MATERIAL_PROGRAM_RESOURCE_ID = tbx::Uuid(0xAA11F0FFU);
        return FALLBACK_MATERIAL_PROGRAM_RESOURCE_ID;
    }

    static const tbx::Texture& get_fallback_texture()
    {
        static const auto FALLBACK_TEXTURE = tbx::Texture(
            tbx::Size {1, 1},
            tbx::TextureWrap::REPEAT,
            tbx::TextureFilter::LINEAR,
            tbx::TextureFormat::RGBA,
            tbx::TextureMipmaps::DISABLED,
            tbx::TextureCompression::DISABLED,
            std::vector<tbx::Pixel> {255, 255, 255, 255});
        return FALLBACK_TEXTURE;
    }

    static const tbx::Texture& get_flat_normal_texture_data()
    {
        static const auto FLAT_NORMAL_TEXTURE = tbx::Texture(
            tbx::Size {1, 1},
            tbx::TextureWrap::REPEAT,
            tbx::TextureFilter::LINEAR,
            tbx::TextureFormat::RGB,
            tbx::TextureMipmaps::DISABLED,
            tbx::TextureCompression::DISABLED,
            std::vector<tbx::Pixel> {128, 128, 255});
        return FLAT_NORMAL_TEXTURE;
    }

    OpenGlMaterialParams create_magenta_fallback_material_params(const tbx::Handle& material_handle)
    {
        auto fallback_material = OpenGlMaterialParams();
        fallback_material.material_handle = material_handle;
        fallback_material.parameters.push_back(
            tbx::MaterialParameter("color", tbx::Color(1.0F, 0.0F, 1.0F, 1.0F)));
        fallback_material.parameters.push_back(
            tbx::MaterialParameter("emissive", tbx::Color(1.0F, 0.0F, 1.0F, 1.0F)));
        return fallback_material;
    }

    static std::vector<tbx::ShaderSource> fallback_material_shader_sources()
    {
        auto shader_sources = std::vector<tbx::ShaderSource>();
        shader_sources.reserve(2U);
        shader_sources.push_back(
            tbx::ShaderSource(
                "#version 450 core\n"
                "layout(location = 0) in vec3 a_position;\n"
                "uniform mat4 u_view_proj = mat4(1.0);\n"
                "uniform mat4 u_model = mat4(1.0);\n"
                "void main()\n"
                "{\n"
                "    gl_Position = u_view_proj * (u_model * vec4(a_position, 1.0));\n"
                "}\n",
                tbx::ShaderType::VERTEX));
        shader_sources.push_back(
            tbx::ShaderSource(
                "#version 450 core\n"
                "layout(location = 0) out vec4 o_color;\n"
                "void main()\n"
                "{\n"
                "    o_color = vec4(1.0, 0.0, 1.0, 1.0);\n"
                "}\n",
                tbx::ShaderType::FRAGMENT));
        return shader_sources;
    }

    tbx::Uuid get_fallback_texture(OpenGlUploader& resource_manager)
    {
        return resource_manager.add_texture(
            get_fallback_texture(),
            get_fallback_texture_resource_id(),
            true);
    }

    tbx::Uuid get_flat_normal_texture(OpenGlUploader& resource_manager)
    {
        return resource_manager.add_texture(
            get_flat_normal_texture_data(),
            get_flat_normal_texture_resource_id(),
            true);
    }

    tbx::Uuid get_fallback_material(OpenGlUploader& resource_manager)
    {
        auto existing_program = std::shared_ptr<OpenGlShaderProgram> {};
        const auto fallback_program_id = get_fallback_material_resource_id();
        if (resource_manager.try_get<OpenGlShaderProgram>(fallback_program_id, existing_program))
            return fallback_program_id;

        const auto shader_sources = fallback_material_shader_sources();
        auto shaders = std::vector<std::shared_ptr<OpenGlShader>>();
        shaders.reserve(shader_sources.size());
        for (const auto& shader_source : shader_sources)
        {
            auto shader = std::make_shared<OpenGlShader>(shader_source);
            if (!shader->compile())
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: failed to compile fallback magenta shader stage (type {}).",
                    static_cast<int>(shader_source.type));
                return {};
            }
            shaders.emplace_back(std::move(shader));
        }

        const auto shader_program = std::make_shared<OpenGlShaderProgram>(shaders);
        if (shader_program->get_program_id() == 0)
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: fallback magenta shader program failed to initialize.");
            return {};
        }

        return resource_manager.add_material(shader_program, fallback_program_id, true);
    }
}
