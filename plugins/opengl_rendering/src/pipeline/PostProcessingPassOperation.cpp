#include "PostProcessingPassOperation.h"
#include "RenderPipelineFailure.h"
#include "opengl_fallbacks.h"
#include "opengl_resources/opengl_mesh.h"
#include "opengl_resources/opengl_resource_manager.h"
#include "opengl_resources/opengl_texture.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/mesh.h"
#include <algorithm>
#include <glad/glad.h>

namespace opengl_rendering
{
    static std::string get_handle_label(const tbx::Handle& handle)
    {
        if (!handle.get_name().empty())
            return handle.get_name();
        if (handle.get_id().is_valid())
            return tbx::to_string(handle.get_id());
        return "<invalid>";
    }

    static tbx::MaterialConfig get_default_material_config()
    {
        return tbx::MaterialConfig {
            .depth =
                tbx::MaterialDepthConfig {
                    .is_test_enabled = true,
                    .is_write_enabled = true,
                    .is_prepass_enabled = false,
                    .function = tbx::MaterialDepthFunction::Less,
                },
            .transparency =
                tbx::MaterialTransparencyConfig {
                    .blend_mode = tbx::MaterialBlendMode::Opaque,
                },
            .is_two_sided = false,
            .is_cullable = true,
            .shadow_mode = tbx::ShadowMode::Standard,
        };
    }

    static tbx::MaterialConfig resolve_material_config(
        const tbx::MaterialInstance& material_instance,
        const std::shared_ptr<tbx::Material>& material_asset)
    {
        auto config = get_default_material_config();
        if (material_asset)
            config = material_asset->config;

        if (material_instance.has_depth_override_enabled())
            config.depth = material_instance.depth;
        return config;
    }

    static tbx::ParamBindings resolve_material_parameters(
        const tbx::MaterialInstance& material_instance,
        const std::shared_ptr<tbx::Material>& material_asset)
    {
        auto parameters = tbx::ParamBindings {};
        if (material_asset)
            parameters = material_asset->parameters;

        for (const auto& parameter : material_instance.param_overrides.values)
            parameters.set(parameter.name, parameter.data);
        return parameters;
    }

    static tbx::TextureBindings resolve_material_textures(
        const tbx::MaterialInstance& material_instance,
        const std::shared_ptr<tbx::Material>& material_asset)
    {
        auto textures = tbx::TextureBindings {};
        if (material_asset)
            textures = material_asset->textures;

        for (const auto& texture : material_instance.texture_overrides.values)
            textures.set(texture.name, texture.texture);
        return textures;
    }

    static std::shared_ptr<tbx::Mesh> get_fullscreen_quad_mesh_data()
    {
        static auto fullscreen_quad = std::make_shared<tbx::Mesh>(tbx::fullscreen_quad);
        return fullscreen_quad;
    }

    static bool append_material_texture(
        OpenGlMaterialParams& material_params,
        const std::string& binding_name,
        const tbx::TextureInstance& texture_instance,
        OpenGlResourceManager& resource_manager)
    {
        auto texture_id = tbx::Uuid {};
        if (texture_instance.handle.is_valid())
            texture_id = resource_manager.add_texture(texture_instance.handle);
        if (!texture_id.is_valid())
            texture_id = get_fallback_texture(resource_manager);
        if (!texture_id.is_valid())
            return false;

        auto texture_resource = std::shared_ptr<OpenGlTexture>();
        if (!resource_manager.try_get<OpenGlTexture>(texture_id, texture_resource))
            return false;

        material_params.textures.push_back(
            OpenGlMaterialTexture {
                .name = binding_name,
                .texture_id = texture_id,
                .gl_texture_id = texture_resource->get_texture_id(),
                .bindless_handle = texture_resource->get_bindless_handle(),
            });
        return true;
    }

    static void bind_post_process_textures(
        const OpenGlMaterialParams& material,
        std::vector<GLuint>& texture_ids,
        std::vector<GLuint>& previous_texture_ids,
        std::vector<GLuint>& zero_texture_ids,
        std::size_t& last_bound_count)
    {
        texture_ids.clear();
        texture_ids.reserve(material.textures.size());
        for (const auto& texture : material.textures)
            texture_ids.push_back(static_cast<GLuint>(texture.gl_texture_id));

        const auto current_count = texture_ids.size();
        const auto is_same_layout =
            current_count == last_bound_count && texture_ids == previous_texture_ids;
        if (!is_same_layout && current_count > 0)
            glBindTextures(0, static_cast<GLsizei>(current_count), texture_ids.data());

        if (!is_same_layout && last_bound_count > current_count)
        {
            const auto extra_count = last_bound_count - current_count;
            zero_texture_ids.assign(extra_count, 0U);
            glBindTextures(
                static_cast<GLuint>(current_count),
                static_cast<GLsizei>(extra_count),
                zero_texture_ids.data());
        }

        previous_texture_ids = texture_ids;
        last_bound_count = current_count;
    }

    PostProcessingPassOperation::PostProcessingPassOperation(
        OpenGlResourceManager& resource_manager,
        OpenGlGBuffer& gbuffer)
        : _resource_manager(resource_manager)
        , _gbuffer(gbuffer)
    {
    }

    PostProcessingPassOperation::~PostProcessingPassOperation() noexcept
    {
        destroy_scratch_targets();
    }

    void PostProcessingPassOperation::execute(const std::any& payload)
    {
        const auto& frame_context = std::any_cast<const OpenGlFrameContext&>(payload);
        if (!frame_context.has_post_processing || !frame_context.post_processing.is_enabled)
            return;
        if (frame_context.render_size.width == 0U || frame_context.render_size.height == 0U)
            return;
        if (!ensure_scratch_targets(frame_context.render_size))
        {
            TBX_TRACE_WARNING("OpenGL rendering: failed to allocate post-processing targets.");
            report_render_pipeline_failure();
            return;
        }

        auto fullscreen_quad_mesh = std::shared_ptr<OpenGlMesh> {};
        const auto fullscreen_quad_key = _resource_manager.add_dynamic_mesh(
            tbx::DynamicMesh(get_fullscreen_quad_mesh_data()),
            true);
        if (!fullscreen_quad_key.is_valid()
            || !_resource_manager.try_get<OpenGlMesh>(fullscreen_quad_key, fullscreen_quad_mesh))
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: failed to prepare post-processing fullscreen mesh.");
            report_render_pipeline_failure();
            return;
        }

        auto source_texture = _gbuffer.get_final_color_texture();
        auto ping_pong_target_index = std::size_t {0U};
        auto did_apply_effect = false;
        auto texture_ids = std::vector<GLuint> {};
        auto previous_texture_ids = std::vector<GLuint> {};
        auto zero_texture_ids = std::vector<GLuint> {};
        std::size_t last_bound_texture_count = 0U;

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);

        for (const auto& effect : frame_context.post_processing.effects)
        {
            if (!effect.is_enabled)
                continue;

            const auto& effect_handle = effect.material.get_handle();
            if (effect_handle.get_name().empty() && !effect_handle.get_id().is_valid())
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: post-processing effect skipped because its material "
                    "handle is invalid.");
                continue;
            }

            const auto shader_program_key = _resource_manager.add_material(effect.material);
            if (!shader_program_key.is_valid())
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: failed to cache post-processing shader program for "
                    "material '{}'. Effect skipped.",
                    get_handle_label(effect_handle));
                continue;
            }

            auto shader_program = std::shared_ptr<OpenGlShaderProgram> {};
            if (!_resource_manager.try_get<OpenGlShaderProgram>(shader_program_key, shader_program))
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: cached post-processing shader program '{}' was not "
                    "available for material '{}'. Effect skipped.",
                    shader_program_key.value,
                    get_handle_label(effect_handle));
                continue;
            }

            const auto material_asset =
                _resource_manager.get_material_asset(effect.material.get_handle());
            const auto material_config = resolve_material_config(effect.material, material_asset);
            auto material_parameters = resolve_material_parameters(effect.material, material_asset);
            const auto material_textures =
                resolve_material_textures(effect.material, material_asset);
            material_parameters.set("blend", std::clamp(effect.blend, 0.0F, 1.0F));

            auto material_params = OpenGlMaterialParams {};
            material_params.material_handle = effect.material.get_handle();
            material_params.render_config = tbx::MaterialRenderConfig {
                .depth = material_config.depth,
                .transparency = material_config.transparency,
            };
            material_params.parameters.reserve(material_parameters.values.size());
            for (const auto& [name, value] : material_parameters.values)
                material_params.parameters.push_back(tbx::MaterialParameter(name, value));

            material_params.textures.reserve(material_textures.values.size() + 1U);
            for (const auto& texture_binding : material_textures.values)
                append_material_texture(
                    material_params,
                    texture_binding.name,
                    texture_binding.texture,
                    _resource_manager);

            material_params.textures.push_back(
                OpenGlMaterialTexture {
                    .name = "scene_color",
                    .gl_texture_id = source_texture,
                });

            const auto target_framebuffer = _scratch_framebuffers[ping_pong_target_index];
            const auto target_texture = _scratch_textures[ping_pong_target_index];
            glBindFramebuffer(GL_FRAMEBUFFER, target_framebuffer);
            glViewport(
                0,
                0,
                static_cast<GLsizei>(frame_context.render_size.width),
                static_cast<GLsizei>(frame_context.render_size.height));

            shader_program->bind();
            bind_post_process_textures(
                material_params,
                texture_ids,
                previous_texture_ids,
                zero_texture_ids,
                last_bound_texture_count);
            if (!shader_program->try_upload(material_params))
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: failed to upload post-processing material parameters "
                    "for material '{}'. Effect skipped for this frame.",
                    get_handle_label(effect_handle));
                continue;
            }

            fullscreen_quad_mesh->bind();
            fullscreen_quad_mesh->draw_bound();

            source_texture = target_texture;
            ping_pong_target_index = 1U - ping_pong_target_index;
            did_apply_effect = true;
        }

        glDepthMask(GL_TRUE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (did_apply_effect)
            _gbuffer.apply_to_final_color(source_texture);
    }

    void PostProcessingPassOperation::destroy_scratch_targets() noexcept
    {
        for (auto& framebuffer : _scratch_framebuffers)
        {
            if (framebuffer == 0U)
                continue;
            glDeleteFramebuffers(1, &framebuffer);
            framebuffer = 0U;
        }

        for (auto& texture : _scratch_textures)
        {
            if (texture == 0U)
                continue;
            glDeleteTextures(1, &texture);
            texture = 0U;
        }

        _scratch_size = {0U, 0U};
    }

    bool PostProcessingPassOperation::ensure_scratch_targets(const tbx::Size& size)
    {
        if (_scratch_size.width == size.width && _scratch_size.height == size.height
            && _scratch_framebuffers.size() == 2U && _scratch_textures.size() == 2U
            && _scratch_framebuffers[0] != 0U && _scratch_framebuffers[1] != 0U
            && _scratch_textures[0] != 0U && _scratch_textures[1] != 0U)
            return true;

        destroy_scratch_targets();
        _scratch_size = size;
        _scratch_textures.resize(2U, 0U);
        _scratch_framebuffers.resize(2U, 0U);

        glCreateTextures(GL_TEXTURE_2D, 2, _scratch_textures.data());
        for (const auto texture : _scratch_textures)
        {
            glTextureStorage2D(
                texture,
                1,
                GL_RGBA8,
                static_cast<GLsizei>(size.width),
                static_cast<GLsizei>(size.height));
            glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        glCreateFramebuffers(2, _scratch_framebuffers.data());
        for (std::size_t index = 0U; index < _scratch_framebuffers.size(); ++index)
        {
            glNamedFramebufferTexture(
                _scratch_framebuffers[index],
                GL_COLOR_ATTACHMENT0,
                _scratch_textures[index],
                0);
            glNamedFramebufferDrawBuffer(_scratch_framebuffers[index], GL_COLOR_ATTACHMENT0);

            const auto status =
                glCheckNamedFramebufferStatus(_scratch_framebuffers[index], GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE)
                return false;
        }

        return true;
    }
}
