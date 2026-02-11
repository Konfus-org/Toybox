#include "opengl_render_pipeline.h"
#include "opengl_resources/opengl_resource.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/matrices.h"
#include "tbx/math/transform.h"
#include <any>
#include <glad/glad.h>
#include <vector>

namespace tbx::plugins
{
    class OpenGlGeometryOperation final : public OpenGlRenderOperation
    {
      public:
        explicit OpenGlGeometryOperation(OpenGlResourceManager& resource_manager)
            : _resource_manager(&resource_manager)
        {
        }

        void execute_with_frame_context(const OpenGlRenderFrameContext& frame_context) override
        {
            TBX_ASSERT(
                _resource_manager != nullptr,
                "OpenGL rendering: geometry operation requires a resource manager.");
            TBX_ASSERT(
                frame_context.render_target != nullptr,
                "OpenGL rendering: geometry operation requires a render target.");

            auto render_target_scope = GlResourceScope(*frame_context.render_target);
            glViewport(
                0,
                0,
                static_cast<GLsizei>(frame_context.render_resolution.width),
                static_cast<GLsizei>(frame_context.render_resolution.height));
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const auto view_projection = get_view_projection_matrix(frame_context.camera_view);
            const auto camera_position = get_camera_position(frame_context.camera_view);
            for (auto& entity : frame_context.camera_view.in_view_static_entities)
                draw_entity(entity, view_projection, camera_position);
            for (auto& entity : frame_context.camera_view.in_view_dynamic_entities)
                draw_entity(entity, view_projection, camera_position);
        }

      private:
        Mat4 get_view_projection_matrix(const OpenGlCameraView& camera_view) const
        {
            if (!camera_view.camera_entity.has_component<Camera>())
                return Mat4(1.0f);

            const auto& camera = camera_view.camera_entity.get_component<Camera>();
            const auto camera_rotation = get_camera_rotation(camera_view);
            const auto camera_position = get_camera_position(camera_view);
            return camera.get_view_projection_matrix(camera_position, camera_rotation);
        }

        Vec3 get_camera_position(const OpenGlCameraView& camera_view) const
        {
            if (!camera_view.camera_entity.has_component<Transform>())
                return Vec3(0.0f);

            const auto& camera_transform = camera_view.camera_entity.get_component<Transform>();
            return camera_transform.position;
        }

        Quat get_camera_rotation(const OpenGlCameraView& camera_view) const
        {
            if (!camera_view.camera_entity.has_component<Transform>())
                return Quat(1.0f, 0.0f, 0.0f, 0.0f);

            const auto& camera_transform = camera_view.camera_entity.get_component<Transform>();
            return camera_transform.rotation;
        }

        void upload_frame_uniforms(
            OpenGlShaderProgram& shader_program,
            const Mat4& view_projection,
            const Vec3& camera_position) const
        {
            shader_program.upload(
                ShaderUniform {
                    .name = "u_view_proj",
                    .data = view_projection,
                });
            shader_program.upload(
                ShaderUniform {
                    .name = "u_camera_pos",
                    .data = camera_position,
                });
        }

        void upload_entity_uniforms(
            OpenGlShaderProgram& shader_program,
            const Entity& entity,
            const Renderer& renderer,
            const OpenGlDrawResources& draw_resources) const
        {
            auto transform = Transform();
            if (entity.has_component<Transform>())
                transform = entity.get_component<Transform>();

            const auto model_matrix = build_transform_matrix(transform);
            const auto normal_matrix = inverse_transpose(Mat3(model_matrix));
            shader_program.upload(
                ShaderUniform {
                    .name = "u_model",
                    .data = model_matrix,
                });
            shader_program.upload(
                ShaderUniform {
                    .name = "u_normal_matrix",
                    .data = normal_matrix,
                });

            auto merged_uniforms = std::vector<ShaderUniform>(
                draw_resources.shader_parameters.begin(),
                draw_resources.shader_parameters.end());
            for (const auto& entity_uniform : renderer.material_overrides.get_uniforms())
            {
                auto did_override = false;
                for (auto& merged_uniform : merged_uniforms)
                {
                    if (merged_uniform.name != entity_uniform.name)
                        continue;

                    merged_uniform = entity_uniform;
                    did_override = true;
                    break;
                }

                if (!did_override)
                    merged_uniforms.push_back(entity_uniform);
            }

            for (const auto& merged_uniform : merged_uniforms)
                shader_program.upload(merged_uniform);
        }

        void bind_textures(
            const OpenGlDrawResources& draw_resources,
            std::vector<GlResourceScope>& out_scopes) const
        {
            for (const auto& texture_binding : draw_resources.textures)
            {
                if (!texture_binding.texture)
                    continue;

                out_scopes.push_back(GlResourceScope(*texture_binding.texture));
            }
        }

        void apply_culling(const Renderer& renderer) const
        {
            if (renderer.is_two_sided)
            {
                glDisable(GL_CULL_FACE);
                return;
            }

            glEnable(GL_CULL_FACE);
        }

        void draw_entity(Entity& entity, const Mat4& view_projection, const Vec3& camera_position)
            const
        {
            TBX_ASSERT(
                entity.has_component<Renderer>(),
                "OpenGL rendering: geometry view entity requires Renderer component.");

            const auto& renderer = entity.get_component<Renderer>();
            auto draw_resources = OpenGlDrawResources {};
            if (!_resource_manager->try_load(entity, draw_resources))
                return;

            if (!draw_resources.mesh || !draw_resources.shader_program)
                return;

            apply_culling(renderer);

            auto resource_scopes = std::vector<GlResourceScope> {};
            resource_scopes.reserve(draw_resources.textures.size() + 2);
            resource_scopes.push_back(GlResourceScope(*draw_resources.shader_program));
            resource_scopes.push_back(GlResourceScope(*draw_resources.mesh));
            bind_textures(draw_resources, resource_scopes);

            upload_frame_uniforms(*draw_resources.shader_program, view_projection, camera_position);
            upload_entity_uniforms(
                *draw_resources.shader_program,
                entity,
                renderer,
                draw_resources);
            draw_resources.mesh->draw();
        }

      private:
        OpenGlResourceManager* _resource_manager = nullptr;
    };

    class OpenGlPresentOperation final : public OpenGlRenderOperation
    {
      public:
        void execute_with_frame_context(const OpenGlRenderFrameContext& frame_context) override
        {
            TBX_ASSERT(
                frame_context.render_target != nullptr,
                "OpenGL rendering: present operation requires a render target.");

            glBindFramebuffer(
                GL_READ_FRAMEBUFFER,
                frame_context.render_target->get_framebuffer_id());
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(
                0,
                0,
                static_cast<GLint>(frame_context.render_resolution.width),
                static_cast<GLint>(frame_context.render_resolution.height),
                0,
                0,
                static_cast<GLint>(frame_context.viewport_size.width),
                static_cast<GLint>(frame_context.viewport_size.height),
                GL_COLOR_BUFFER_BIT,
                GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    };

    void OpenGlRenderOperation::execute(const std::any& payload)
    {
        const auto* frame_context = std::any_cast<OpenGlRenderFrameContext>(&payload);
        TBX_ASSERT(
            frame_context != nullptr,
            "OpenGL rendering: render operation requires OpenGlRenderFrameContext payload.");
        execute_with_frame_context(*frame_context);
    }

    OpenGlRenderPipeline::OpenGlRenderPipeline(AssetManager& asset_manager)
        : _resource_manager(asset_manager)
    {
        add_operation(std::make_unique<OpenGlGeometryOperation>(_resource_manager));
        add_operation(std::make_unique<OpenGlPresentOperation>());
    }

    OpenGlRenderPipeline::~OpenGlRenderPipeline() noexcept
    {
        clear_resource_caches();
        clear_operations();
    }

    void OpenGlRenderPipeline::execute(const std::any& payload)
    {
        const auto* frame_context = std::any_cast<OpenGlRenderFrameContext>(&payload);
        TBX_ASSERT(
            frame_context != nullptr,
            "OpenGL rendering: execute() requires OpenGlRenderFrameContext payload.");
        TBX_ASSERT(
            frame_context->render_resolution.width > 0
                && frame_context->render_resolution.height > 0,
            "OpenGL rendering: render resolution must be greater than zero.");
        TBX_ASSERT(
            frame_context->viewport_size.width > 0 && frame_context->viewport_size.height > 0,
            "OpenGL rendering: viewport size must be greater than zero.");
        TBX_ASSERT(
            frame_context->render_target != nullptr,
            "OpenGL rendering: frame context requires a render target framebuffer.");

        _resource_manager.unload_unreferenced();
        Pipeline::execute(payload);
    }

    void OpenGlRenderPipeline::clear_resource_caches()
    {
        _resource_manager.clear();
    }
}
