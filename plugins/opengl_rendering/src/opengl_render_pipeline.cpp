#include "opengl_render_pipeline.h"
#include "opengl_deferred_lighting_pass.h"
#include "opengl_post_process_pass.h"
#include "opengl_sky_pass.h"
#include "opengl_resources/opengl_resource.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/renderer.h"
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
                frame_context.gbuffer_target != nullptr,
                "OpenGL rendering: geometry operation requires a gbuffer target.");

            auto render_target_scope = GlResourceScope(*frame_context.gbuffer_target);
            glViewport(
                0,
                0,
                static_cast<GLsizei>(frame_context.render_resolution.width),
                static_cast<GLsizei>(frame_context.render_resolution.height));

            const auto view_projection =
                get_view_projection_matrix(
                    frame_context.camera_view,
                    frame_context.render_resolution);
            for (const auto& entity : frame_context.camera_view.in_view_static_entities)
                draw_entity(entity, view_projection);
            for (const auto& entity : frame_context.camera_view.in_view_dynamic_entities)
                draw_entity(entity, view_projection);
        }

      private:
        Mat4 get_view_projection_matrix(
            const OpenGlCameraView& camera_view,
            const Size& render_resolution) const
        {
            if (!camera_view.camera_entity.has_component<Camera>())
                return Mat4(1.0f);

            auto& camera = camera_view.camera_entity.get_component<Camera>();
            camera.set_aspect(render_resolution.get_aspect_ratio());
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
            const Mat4& view_projection) const
        {
            shader_program.upload(
                MaterialParameter {
                    .name = "u_view_proj",
                    .data = view_projection,
                });
        }

        void upload_model_uniform(
            OpenGlShaderProgram& shader_program,
            const Entity& entity) const
        {
            auto transform = Transform();
            if (entity.has_component<Transform>())
                transform = entity.get_component<Transform>();

            const auto model_matrix = build_transform_matrix(transform);
            shader_program.upload(
                MaterialParameter {
                    .name = "u_model",
                    .data = model_matrix,
                });
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

        void upload_material_uniforms(
            OpenGlShaderProgram& shader_program,
            const OpenGlDrawResources& draw_resources) const
        {
            for (const auto& uniform : draw_resources.shader_parameters)
                shader_program.try_upload(uniform);
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

        void draw_entity(const Entity& entity, const Mat4& view_projection) const
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
            TBX_ASSERT(
                draw_resources.shader_program->get_program_id() != 0,
                "OpenGL rendering: draw call requires a valid shader program.");

            apply_culling(renderer);

            auto resource_scopes = std::vector<GlResourceScope> {};
            resource_scopes.reserve(draw_resources.textures.size() + 2);
            resource_scopes.push_back(GlResourceScope(*draw_resources.shader_program));
            resource_scopes.push_back(GlResourceScope(*draw_resources.mesh));
            bind_textures(draw_resources, resource_scopes);

            upload_frame_uniforms(*draw_resources.shader_program, view_projection);
            upload_material_uniforms(*draw_resources.shader_program, draw_resources);
            upload_model_uniform(*draw_resources.shader_program, entity);
            draw_resources.mesh->draw(draw_resources.use_tesselation);
        }

      private:
        OpenGlResourceManager* _resource_manager = nullptr;
    };

    class OpenGlSkyOperation final : public OpenGlRenderOperation
    {
      public:
        explicit OpenGlSkyOperation(OpenGlResourceManager& resource_manager)
            : _resource_manager(&resource_manager)
        {
        }

        void execute_with_frame_context(const OpenGlRenderFrameContext& frame_context) override
        {
            TBX_ASSERT(
                _resource_manager != nullptr,
                "OpenGL rendering: sky operation requires a resource manager.");
            _pass.execute(frame_context, *_resource_manager);
        }

      private:
        OpenGlResourceManager* _resource_manager = nullptr;
        OpenGlSkyPass _pass = {};
    };

    class OpenGlDeferredLightingOperation final : public OpenGlRenderOperation
    {
      public:
        void execute_with_frame_context(const OpenGlRenderFrameContext& frame_context) override
        {
            _pass.execute(frame_context);
        }

      private:
        OpenGlDeferredLightingPass _pass = {};
    };

    class OpenGlPostProcessOperation final : public OpenGlRenderOperation
    {
      public:
        explicit OpenGlPostProcessOperation(OpenGlResourceManager& resource_manager)
            : _resource_manager(&resource_manager)
        {
        }

        void execute_with_frame_context(const OpenGlRenderFrameContext& frame_context) override
        {
            TBX_ASSERT(
                _resource_manager != nullptr,
                "OpenGL rendering: post-process operation requires a resource manager.");
            _pass.execute(frame_context, *_resource_manager);
        }

      private:
        OpenGlResourceManager* _resource_manager = nullptr;
        OpenGlPostProcessPass _pass = {};
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
        add_operation(std::make_unique<OpenGlSkyOperation>(_resource_manager));
        add_operation(std::make_unique<OpenGlGeometryOperation>(_resource_manager));
        add_operation(std::make_unique<OpenGlDeferredLightingOperation>());
        add_operation(std::make_unique<OpenGlPostProcessOperation>(_resource_manager));
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
            frame_context->gbuffer_target != nullptr,
            "OpenGL rendering: frame context requires a gbuffer target framebuffer.");
        TBX_ASSERT(
            frame_context->lighting_target != nullptr,
            "OpenGL rendering: frame context requires a lighting target framebuffer.");

        _resource_manager.unload_unreferenced();
        Pipeline::execute(payload);
    }

    void OpenGlRenderPipeline::clear_resource_caches()
    {
        _resource_manager.clear();
    }
}
