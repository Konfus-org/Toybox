#include "opengl_render_pipeline.h"
#include "opengl_resources/opengl_resource.h"
#include "tbx/debugging/macros.h"
#include <any>
#include <glad/glad.h>

namespace tbx::plugins
{
    class OpenGlGeometryOperation final : public OpenGlRenderOperation
    {
      public:
        void execute_with_frame_context(const OpenGlRenderFrameContext& frame_context) override
        {
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
        }
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
        add_operation(std::make_unique<OpenGlGeometryOperation>());
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
            frame_context->render_resolution.width > 0 && frame_context->render_resolution.height > 0,
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
