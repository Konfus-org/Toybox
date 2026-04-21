#include "opengl_renderer.h"
#include "opengl_resources/opengl_bindless.h"
#include "opengl_resources/opengl_mesh.h"
#include "opengl_resources/opengl_shader.h"
#include "opengl_resources/opengl_texture.h"
#include "tbx/common/string_utils.h"
#include "tbx/debugging/macros.h"
#include <algorithm>
#include <array>
#include <glad/glad.h>

namespace opengl_rendering
{
    namespace
    {

        void gl_message_callback(
            GLenum source,
            GLenum type,
            GLuint id,
            GLenum severity,
            const GLsizei length,
            const GLchar* message,
            const void*)
        {
            switch (severity)
            {
                case GL_DEBUG_SEVERITY_HIGH:
                case GL_DEBUG_SEVERITY_MEDIUM:
                case GL_DEBUG_SEVERITY_LOW:
                {
                    TBX_TRACE_WARNING(
                        "GL debug message (source: {}, type: {}, id: {}, severity: {}) - {}:",
                        source,
                        type,
                        id,
                        severity,
                        message,
                        std::string_view(message, length));
                    break;
                }
                default:
                    break;
            }
        }

        bool should_warn_about_integrated_or_software_gpu(
            const std::string_view vendor_text,
            const std::string_view renderer_text)
        {
            const auto is_software_renderer =
                tbx::contains_case_insensitive(renderer_text, "llvmpipe")
                || tbx::contains_case_insensitive(renderer_text, "softpipe")
                || tbx::contains_case_insensitive(renderer_text, "software rasterizer");
            if (is_software_renderer)
                return true;

            const auto is_intel_vendor = tbx::contains_case_insensitive(vendor_text, "intel");
            const auto is_discrete_arc = tbx::contains_case_insensitive(renderer_text, "arc");
            if (is_intel_vendor && !is_discrete_arc)
                return true;

            return tbx::contains_case_insensitive(renderer_text, "integrated");
        }

        void maybe_warn_about_gpu_selection(
            const std::string_view vendor_text,
            const std::string_view renderer_text)
        {
            if (!should_warn_about_integrated_or_software_gpu(vendor_text, renderer_text))
                return;

            TBX_TRACE_WARNING(
                "OpenGL rendering appears to be running on an integrated/software GPU "
                "(vendor='{}', renderer='{}'). If a discrete GPU is available, prefer launching "
                "this app on the high-performance GPU. This can reduce performance and may affect "
                "shadow quality or availability on some drivers.",
                vendor_text,
                renderer_text);
        }

        bool try_append_material_texture(
            OpenGlMaterialParams& material_params,
            const std::string& binding_name,
            const tbx::TextureInstance& texture_instance,
            const tbx::RenderFallbacks& fallbacks,
            OpenGlResources& resources)
        {
            if (texture_instance.handle.is_valid())
            {
                const auto texture_id = texture_instance.handle.get_id();
                auto texture_resource = std::shared_ptr<OpenGlTexture> {};
                if (!texture_id.is_valid()
                    || !resources.try_get<OpenGlTexture>(texture_id, texture_resource))
                {
                    return false;
                }

                material_params.textures.push_back(
                    OpenGlMaterialTexture {
                        .name = binding_name,
                        .texture_id = texture_id,
                        .gl_texture_id = texture_resource->get_texture_id(),
                        .bindless_handle = texture_resource->get_bindless_handle(),
                    });
                return true;
            }

            auto fallback_texture = std::shared_ptr<OpenGlTexture> {};
            if (!fallbacks.white_texture_resource.is_valid()
                || !resources.try_get<OpenGlTexture>(fallbacks.white_texture_resource, fallback_texture))
            {
                return false;
            }

            material_params.textures.push_back(
                OpenGlMaterialTexture {
                    .name = binding_name,
                    .texture_id = fallbacks.white_texture_resource,
                    .gl_texture_id = fallback_texture->get_texture_id(),
                    .bindless_handle = fallback_texture->get_bindless_handle(),
                });
            return true;
        }

        OpenGlMaterialParams build_material_params(
            const tbx::Uuid& material_resource,
            const tbx::MaterialConfig& material_config,
            const tbx::ParamBindings& material_parameters,
            const tbx::TextureBindings& material_textures,
            const tbx::RenderFallbacks& fallbacks,
            OpenGlResources& resources)
        {
            auto material_params = OpenGlMaterialParams {};
            material_params.material_handle = tbx::Handle(material_resource);
            for (const auto& [name, value] : material_parameters.values)
                material_params.parameters.push_back(tbx::MaterialParameter(name, value));

            material_params.render_config = tbx::MaterialRenderConfig {
                .depth = material_config.depth,
                .transparency = material_config.transparency,
            };

            for (const auto& binding : material_textures.values)
                try_append_material_texture(
                    material_params,
                    binding.name,
                    binding.texture,
                    fallbacks,
                    resources);

            return material_params;
        }

        GLenum to_gl_depth_function(const tbx::MaterialDepthFunction function)
        {
            switch (function)
            {
                case tbx::MaterialDepthFunction::Less:
                    return GL_LESS;
                case tbx::MaterialDepthFunction::LessEqual:
                    return GL_LEQUAL;
                case tbx::MaterialDepthFunction::Always:
                    return GL_ALWAYS;
                default:
                    return GL_LESS;
            }
        }

        void apply_depth_config(const tbx::MaterialDepthConfig& depth_config)
        {
            if (depth_config.is_test_enabled)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);

            glDepthMask(depth_config.is_write_enabled ? GL_TRUE : GL_FALSE);
            glDepthFunc(to_gl_depth_function(depth_config.function));
        }

        void bind_material_textures(const OpenGlMaterialParams& material)
        {
            auto texture_ids = std::vector<GLuint> {};
            for (const auto& texture : material.textures)
                texture_ids.push_back(static_cast<GLuint>(texture.gl_texture_id));
            if (!texture_ids.empty())
                glBindTextures(0, static_cast<GLsizei>(texture_ids.size()), texture_ids.data());
        }

        GLuint ensure_fullscreen_vao()
        {
            static auto fullscreen_vao = GLuint {0U};
            if (fullscreen_vao == 0U)
                glCreateVertexArrays(1, &fullscreen_vao);
            return fullscreen_vao;
        }

        bool try_get_scratch_framebuffer(
            const tbx::Uuid& scratch_texture_resource,
            OpenGlResources& resources,
            GLuint& out_texture,
            GLuint& out_framebuffer)
        {
            out_texture = 0U;
            out_framebuffer = 0U;
            auto texture = std::shared_ptr<OpenGlTexture> {};
            if (!scratch_texture_resource.is_valid()
                || !resources.try_get<OpenGlTexture>(scratch_texture_resource, texture))
            {
                return false;
            }
            out_texture = texture->get_texture_id();

            glCreateFramebuffers(1, &out_framebuffer);
            glNamedFramebufferTexture(out_framebuffer, GL_COLOR_ATTACHMENT0, out_texture, 0);
            glNamedFramebufferDrawBuffer(out_framebuffer, GL_COLOR_ATTACHMENT0);
            const auto status = glCheckNamedFramebufferStatus(out_framebuffer, GL_FRAMEBUFFER);
            if (status == GL_FRAMEBUFFER_COMPLETE)
                return true;

            glDeleteFramebuffers(1, &out_framebuffer);
            out_framebuffer = 0U;
            return false;
        }

        using RenderMesh = tbx::RenderDrawItem;

        bool draw_mesh(
            OpenGlResources& resources,
            const tbx::Mat4& view_projection,
            const tbx::RenderUniformNames& uniforms,
            const tbx::RenderFallbacks& fallbacks,
            const RenderMesh& draw_item)
        {
            const auto shader_program_key =
                draw_item.material_resource.is_valid() ? draw_item.material_resource
                                                       : fallbacks.material_resource;
            const auto mesh_resource = draw_item.mesh_resource.is_valid() ? draw_item.mesh_resource
                                                                           : fallbacks.mesh_resource;
            if (!shader_program_key.is_valid() || !mesh_resource.is_valid())
                return false;

            auto shader_program = std::shared_ptr<OpenGlShaderProgram> {};
            auto mesh = std::shared_ptr<OpenGlMesh> {};
            if (!resources.try_get<OpenGlShaderProgram>(shader_program_key, shader_program)
                || !resources.try_get<OpenGlMesh>(mesh_resource, mesh))
            {
                return false;
            }

            const auto material_params = build_material_params(
                draw_item.material_resource,
                draw_item.material_config,
                draw_item.material_parameters,
                draw_item.material_textures,
                fallbacks,
                resources);

            // Keep render state simple and explicit per draw item.
            if (draw_item.material_config.is_two_sided)
                glDisable(GL_CULL_FACE);
            else
                glEnable(GL_CULL_FACE);

            apply_depth_config(material_params.render_config.depth);
            shader_program->bind();
            shader_program->try_upload(tbx::MaterialParameter(uniforms.view_projection, view_projection));
            shader_program->try_upload(tbx::MaterialParameter(uniforms.model, draw_item.transform));
            bind_material_textures(material_params);
            shader_program->try_upload(material_params);
            mesh->bind();
            mesh->draw_bound();
            return true;
        }
    }

    OpenGlRenderer::OpenGlRenderer(tbx::AssetManager& asset_manager, tbx::JobSystem& job_system)
        : _asset_manager(asset_manager)
        , _job_system(job_system)
        , _resources(_asset_manager)
    {
    }

    OpenGlRenderer::~OpenGlRenderer() noexcept = default;

    tbx::GraphicsApi OpenGlRenderer::get_api() const
    {
        return tbx::GraphicsApi::OPEN_GL;
    }

    tbx::Result OpenGlRenderer::initialize(const tbx::GraphicsProcAddress loader)
    {
        initialize_runtime(loader);
        return {};
    }

    tbx::Result OpenGlRenderer::upload(const tbx::Mesh& mesh, tbx::Uuid& out_resource_uuid)
    {
        auto resource_uuid = out_resource_uuid;
        if (!resource_uuid.is_valid())
            resource_uuid = tbx::Uuid::generate();
        const auto uploaded_resource_uuid = _resources.upload_mesh(mesh, resource_uuid);
        if (!uploaded_resource_uuid.is_valid())
            return tbx::Result(false, "OpenGL rendering: failed to upload mesh resource.");

        out_resource_uuid = uploaded_resource_uuid;
        return {};
    }

    tbx::Result OpenGlRenderer::upload(const tbx::Material& material, tbx::Uuid& out_resource_uuid)
    {
        auto shader_resources = std::vector<std::shared_ptr<OpenGlShader>> {};
        auto try_append_shader = [this, &shader_resources](const tbx::Handle& shader_handle)
        {
            if (!shader_handle.is_valid())
                return true;

            const auto shader = _asset_manager.load<tbx::Shader>(shader_handle);
            if (!shader)
                return false;

            for (const auto& source : shader->sources)
            {
                auto shader_resource = std::make_shared<OpenGlShader>(source);
                if (!shader_resource->compile())
                    return false;
                shader_resources.emplace_back(std::move(shader_resource));
            }

            return !shader->sources.empty();
        };

        const auto has_compute = material.program.compute.is_valid();
        const auto appended_compute = try_append_shader(material.program.compute);
        const auto appended_vertex = has_compute ? true : try_append_shader(material.program.vertex);
        const auto appended_fragment = has_compute ? true : try_append_shader(material.program.fragment);
        const auto appended_tesselation =
            has_compute ? true : try_append_shader(material.program.tesselation);
        const auto appended_geometry =
            has_compute ? true : try_append_shader(material.program.geometry);
        const auto has_valid_shader_set =
            (has_compute && appended_compute)
            || (!has_compute && appended_vertex && appended_fragment && appended_tesselation
                && appended_geometry);
        if (!has_valid_shader_set)
            return tbx::Result(false, "OpenGL rendering: failed to compile material shader set.");

        const auto shader_program = std::make_shared<OpenGlShaderProgram>(shader_resources);
        if (shader_program->get_program_id() == 0)
            return tbx::Result(false, "OpenGL rendering: failed to link shader program.");

        auto resource_uuid = out_resource_uuid;
        if (!resource_uuid.is_valid())
            resource_uuid = tbx::Uuid::generate();
        const auto uploaded_resource_uuid =
            _resources.upload_material(shader_program, resource_uuid);
        if (!uploaded_resource_uuid.is_valid())
            return tbx::Result(false, "OpenGL rendering: failed to upload material resource.");

        out_resource_uuid = uploaded_resource_uuid;
        return {};
    }

    tbx::Result OpenGlRenderer::upload(const tbx::Texture& texture, tbx::Uuid& out_resource_uuid)
    {
        const auto resource_uuid = out_resource_uuid.is_valid() ? out_resource_uuid : tbx::Uuid::generate();
        const auto uploaded_resource_uuid = _resources.upload_texture(texture, resource_uuid);
        if (!uploaded_resource_uuid.is_valid())
            return tbx::Result(false, "OpenGL rendering: failed to upload texture resource.");

        out_resource_uuid = uploaded_resource_uuid;
        return {};
    }

    tbx::Result OpenGlRenderer::upload(
        const tbx::TextureSettings& texture_settings,
        tbx::Uuid& out_resource_uuid)
    {
        const auto resource_uuid = out_resource_uuid.is_valid() ? out_resource_uuid : tbx::Uuid::generate();
        auto texture = tbx::Texture {};
        static_cast<tbx::TextureSettings&>(texture) = texture_settings;
        const auto uploaded_resource_uuid = _resources.upload_texture(texture, resource_uuid);
        if (!uploaded_resource_uuid.is_valid())
            return tbx::Result(false, "OpenGL rendering: failed to upload render texture resource.");

        out_resource_uuid = uploaded_resource_uuid;
        return {};
    }

    tbx::Result OpenGlRenderer::unload(const tbx::Uuid& resource_uuid)
    {
        _resources.unload(resource_uuid);
        return {};
    }

    tbx::Result OpenGlRenderer::begin_draw(
        const tbx::Window& window,
        const tbx::Camera& view,
        const tbx::Size& resolution)
    {
        static_cast<void>(window);
        static_cast<void>(view);
        _render_size = resolution;
        _render_stage = tbx::RenderStage::FINAL_COLOR;
        _is_frame_active = true;

        glViewport(0, 0, static_cast<GLsizei>(resolution.width), static_cast<GLsizei>(resolution.height));

        // Keep exactly one renderer-owned g-buffer alive and resize it per frame.
        _gbuffer.resize(resolution);
        _gbuffer.prepare_geometry_pass();
        return {};
    }

    tbx::RenderPassOutcome OpenGlRenderer::draw_shadows(const tbx::ShadowRenderInfo& shadows)
    {
        if (!_is_frame_active)
            return tbx::RenderPassOutcome::fatal("OpenGL rendering: begin_draw must be called first.");
        if (shadows.draw_items.empty())
            return tbx::RenderPassOutcome::success();
        auto shadow_program = std::shared_ptr<OpenGlShaderProgram> {};
        if (!shadows.shadow_shader_program.is_valid()
            || !_resources.try_get<OpenGlShaderProgram>(shadows.shadow_shader_program, shadow_program))
        {
            return tbx::RenderPassOutcome::fatal("OpenGL rendering: missing shadow shader program resource.");
        }

        _gbuffer.prepare_geometry_pass();
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        shadow_program->bind();

        auto drew_any = false;
        const auto shadow_view_projection = tbx::Mat4(1.0F);
        for (const auto& shadow_item : shadows.draw_items)
        {
            auto mesh = std::shared_ptr<OpenGlMesh> {};
            const auto mesh_resource = shadow_item.mesh_resource.is_valid()
                                           ? shadow_item.mesh_resource
                                           : shadows.fallbacks.mesh_resource;
            if (!mesh_resource.is_valid()
                || !_resources.try_get<OpenGlMesh>(mesh_resource, mesh))
            {
                continue;
            }

            if (shadow_item.is_two_sided)
                glDisable(GL_CULL_FACE);
            else
                glEnable(GL_CULL_FACE);

            shadow_program->try_upload(
                tbx::MaterialParameter("u_light_view_proj", shadow_view_projection));
            shadow_program->try_upload(
                tbx::MaterialParameter("u_model", shadow_item.transform));
            mesh->bind();
            mesh->draw_bound();
            drew_any = true;
        }

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        _gbuffer.prepare_geometry_pass();
        if (!drew_any)
            return tbx::RenderPassOutcome::degraded("OpenGL rendering: shadow pass had no valid draws.");
        return tbx::RenderPassOutcome::success();
    }

    tbx::RenderPassOutcome OpenGlRenderer::draw_geometry(const tbx::GeometryRenderInfo& geo)
    {
        if (!_is_frame_active)
            return tbx::RenderPassOutcome::fatal("OpenGL rendering: begin_draw must be called first.");

        _gbuffer.prepare_geometry_pass();
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        auto drew_any = false;
        for (const auto& draw_item : geo.draw_items)
            drew_any |= draw_mesh(
                _resources,
                geo.view_projection,
                geo.uniforms,
                geo.fallbacks,
                draw_item);

        if (drew_any)
            return tbx::RenderPassOutcome::success();
        return tbx::RenderPassOutcome::degraded("OpenGL rendering: geometry pass had no valid draws.");
    }

    tbx::RenderPassOutcome OpenGlRenderer::draw_lighting(const tbx::LightingRenderInfo& lighting)
    {
        if (!_is_frame_active)
            return tbx::RenderPassOutcome::fatal("OpenGL rendering: begin_draw must be called first.");

        _render_stage = lighting.render_stage;
        const auto source_texture = _gbuffer.get_final_color_texture();
        if (source_texture == 0U)
            return tbx::RenderPassOutcome::fatal("OpenGL rendering: missing g-buffer final color for lighting.");

        auto scratch_texture = GLuint {0U};
        auto scratch_framebuffer = GLuint {0U};
        if (!try_get_scratch_framebuffer(
                lighting.scratch_color_texture,
                _resources,
                scratch_texture,
                scratch_framebuffer))
        {
            return tbx::RenderPassOutcome::fatal("OpenGL rendering: failed to allocate lighting target.");
        }
        auto lighting_program = std::shared_ptr<OpenGlShaderProgram> {};
        if (!lighting.lighting_shader_program.is_valid()
            || !_resources.try_get<OpenGlShaderProgram>(lighting.lighting_shader_program, lighting_program))
        {
            glDeleteFramebuffers(1, &scratch_framebuffer);
            return tbx::RenderPassOutcome::fatal("OpenGL rendering: missing lighting shader program resource.");
        }

        auto accumulated_radiance = tbx::Vec3(0.0F);
        auto ambient_intensity = 0.0F;
        for (const auto& light : lighting.directional_lights)
        {
            accumulated_radiance += light.radiance;
            ambient_intensity += light.ambient_intensity;
        }
        for (const auto& light : lighting.point_lights)
            accumulated_radiance += light.radiance;
        for (const auto& light : lighting.spot_lights)
            accumulated_radiance += light.radiance;
        for (const auto& light : lighting.area_lights)
            accumulated_radiance += light.radiance;

        const auto light_gain = tbx::Vec3(1.0F) + accumulated_radiance * 0.05F;
        const auto light_add = tbx::Vec3(_clear_color.r, _clear_color.g, _clear_color.b)
                               * std::max(ambient_intensity, 0.0F);

        glBindFramebuffer(GL_FRAMEBUFFER, scratch_framebuffer);
        glViewport(0, 0, static_cast<GLsizei>(_render_size.width), static_cast<GLsizei>(_render_size.height));
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        lighting_program->bind();
        glBindTextureUnit(0, source_texture);
        lighting_program->try_upload(tbx::MaterialParameter("u_scene_color", 0));
        lighting_program->try_upload(tbx::MaterialParameter("u_light_gain", light_gain));
        lighting_program->try_upload(tbx::MaterialParameter("u_light_add", light_add));
        glBindVertexArray(ensure_fullscreen_vao());
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        _gbuffer.apply_to_final_color(scratch_texture);
        glDeleteFramebuffers(1, &scratch_framebuffer);
        return tbx::RenderPassOutcome::success();
    }

    tbx::RenderPassOutcome OpenGlRenderer::draw_transparent(const tbx::TransparentRenderInfo& transparency)
    {
        if (!_is_frame_active)
            return tbx::RenderPassOutcome::fatal("OpenGL rendering: begin_draw must be called first.");

        // Draw transparent geometry directly into final color using the incoming draw items only.
        _gbuffer.bind();
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        auto sorted_draws = transparency.draw_items;
        std::sort(
            sorted_draws.begin(),
            sorted_draws.end(),
            [](const tbx::RenderDrawItem& lhs, const tbx::RenderDrawItem& rhs)
            {
                return lhs.camera_distance_squared > rhs.camera_distance_squared;
            });

        auto drew_any = false;
        for (const auto& draw_item : sorted_draws)
            drew_any |= draw_mesh(
                _resources,
                transparency.view_projection,
                transparency.uniforms,
                transparency.fallbacks,
                draw_item);

        glDisable(GL_BLEND);
        _gbuffer.prepare_geometry_pass();
        if (drew_any)
            return tbx::RenderPassOutcome::success();
        return tbx::RenderPassOutcome::degraded("OpenGL rendering: transparent pass had no valid draws.");
    }

    tbx::RenderPassOutcome OpenGlRenderer::apply_post_processing(const tbx::PostProcessingPass& post)
    {
        if (!_is_frame_active)
            return tbx::RenderPassOutcome::fatal("OpenGL rendering: begin_draw must be called first.");
        if (!post.post_processing.has_value() || !post.post_processing->is_enabled)
            return tbx::RenderPassOutcome::success();
        auto post_program = std::shared_ptr<OpenGlShaderProgram> {};
        if (!post.post_shader_program.is_valid()
            || !_resources.try_get<OpenGlShaderProgram>(post.post_shader_program, post_program))
        {
            return tbx::RenderPassOutcome::fatal("OpenGL rendering: missing post shader program resource.");
        }

        auto source_texture = _gbuffer.get_final_color_texture();
        if (source_texture == 0U)
            return tbx::RenderPassOutcome::fatal("OpenGL rendering: missing g-buffer final color for post.");

        auto applied_effect = false;
        for (const auto& effect : post.post_processing->effects)
        {
            if (!effect.is_enabled)
                continue;

            auto scratch_texture = GLuint {0U};
            auto scratch_framebuffer = GLuint {0U};
            if (!try_get_scratch_framebuffer(
                    post.scratch_color_texture,
                    _resources,
                    scratch_texture,
                    scratch_framebuffer))
            {
                return tbx::RenderPassOutcome::fatal("OpenGL rendering: failed to allocate post target.");
            }

            glBindFramebuffer(GL_FRAMEBUFFER, scratch_framebuffer);
            glViewport(0, 0, static_cast<GLsizei>(_render_size.width), static_cast<GLsizei>(_render_size.height));
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDisable(GL_BLEND);
            post_program->bind();
            glBindTextureUnit(0, source_texture);
            post_program->try_upload(tbx::MaterialParameter("u_scene_color", 0));
            post_program->try_upload(
                tbx::MaterialParameter("u_blend", std::clamp(effect.blend, 0.0F, 1.0F)));
            glBindVertexArray(ensure_fullscreen_vao());
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);

            _gbuffer.apply_to_final_color(scratch_texture);
            glDeleteFramebuffers(1, &scratch_framebuffer);
            source_texture = _gbuffer.get_final_color_texture();
            applied_effect = true;
        }

        if (!applied_effect)
            return tbx::RenderPassOutcome::degraded("OpenGL rendering: no enabled post-processing effects.");
        return tbx::RenderPassOutcome::success();
    }

    tbx::Result OpenGlRenderer::clear(const tbx::Color& color)
    {
        if (!_is_frame_active)
            return tbx::Result(false, "OpenGL rendering: begin_draw must be called first.");

        _clear_color = color;
        _gbuffer.prepare_geometry_pass();
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glClearColor(color.r, color.g, color.b, color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return {};
    }

    tbx::Result OpenGlRenderer::end_draw()
    {
        if (!_is_frame_active)
            return tbx::Result(false, "OpenGL rendering: begin_draw must be called first.");

        _gbuffer.present(_render_stage, _render_size);

        // Reset the g-buffer after presentation so each frame starts from clean attachments.
        _gbuffer.prepare_geometry_pass();
        glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        _render_size = {0U, 0U};
        _render_stage = tbx::RenderStage::FINAL_COLOR;
        _is_frame_active = false;
        return {};
    }

    void OpenGlRenderer::initialize_runtime(const tbx::GraphicsProcAddress loader)
    {
        if (_is_runtime_initialized)
            return;

        auto* glad_loader = loader;
        TBX_ASSERT(glad_loader != nullptr, "Context manager provided null loader.");

        const auto load_result = gladLoadGLLoader(glad_loader);
        TBX_ASSERT(load_result != 0, "Failed to initialize GLAD.");
        set_bindless_proc_loader(loader);

        const auto major_version = GLVersion.major;
        const auto minor_version = GLVersion.minor;
        TBX_ASSERT(
            major_version > 4 || (major_version == 4 && minor_version >= 5),
            "Requires OpenGL 4.5 or newer.");

        const auto* vendor_string = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const auto* renderer_string = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const auto* version_string = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const auto* glsl_version_string =
            reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
        const auto* vendor_text = vendor_string != nullptr ? vendor_string : "unknown";
        const auto* renderer_text = renderer_string != nullptr ? renderer_string : "unknown";
        TBX_TRACE_INFO(
            "OpenGL runtime info: vendor='{}', renderer='{}', version='{}', GLSL='{}'.",
            vendor_text,
            renderer_text,
            version_string != nullptr ? version_string : "unknown",
            glsl_version_string != nullptr ? glsl_version_string : "unknown");
        maybe_warn_about_gpu_selection(vendor_text, renderer_text);

#if defined(TBX_DEBUG)
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageControl(
            GL_DONT_CARE,
            GL_DONT_CARE,
            GL_DEBUG_SEVERITY_NOTIFICATION,
            0,
            nullptr,
            GL_FALSE);
        glDebugMessageCallback(gl_message_callback, nullptr);
#endif

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glDisable(GL_BLEND);
        glClearColor(0.07F, 0.08F, 0.11F, 1.0F);

        _is_runtime_initialized = true;
    }
}
