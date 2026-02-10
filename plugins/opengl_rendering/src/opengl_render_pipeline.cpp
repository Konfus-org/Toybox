#include "opengl_render_pipeline.h"
#include "opengl_resources/opengl_resource.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <utility>

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

    void OpenGlRenderOperation::set_frame_context(const OpenGlRenderFrameContext* frame_context)
    {
        _frame_context = frame_context;
    }

    void OpenGlRenderOperation::execute()
    {
        TBX_ASSERT(
            _frame_context != nullptr,
            "OpenGL rendering: render operation requires frame context.");
        execute_with_frame_context(*_frame_context);
    }

    const OpenGlRenderFrameContext& OpenGlRenderOperation::get_frame_context() const
    {
        TBX_ASSERT(
            _frame_context != nullptr,
            "OpenGL rendering: render operation requires frame context.");
        return *_frame_context;
    }

    OpenGlRenderPipeline::OpenGlRenderPipeline()
    {
        add_operation(std::make_unique<OpenGlGeometryOperation>());
        add_operation(std::make_unique<OpenGlPresentOperation>());
    }

    OpenGlRenderPipeline::~OpenGlRenderPipeline() noexcept
    {
        clear_operations();
    }

    void OpenGlRenderPipeline::set_frame_context(const OpenGlRenderFrameContext& frame_context)
    {
        _current_frame_context = frame_context;
    }

    void OpenGlRenderPipeline::execute()
    {
        TBX_ASSERT(
            _current_frame_context.has_value(),
            "OpenGL rendering: frame context must be set before execute().");
        TBX_ASSERT(
            _current_frame_context->render_resolution.width > 0
                && _current_frame_context->render_resolution.height > 0,
            "OpenGL rendering: render resolution must be greater than zero.");
        TBX_ASSERT(
            _current_frame_context->viewport_size.width > 0
                && _current_frame_context->viewport_size.height > 0,
            "OpenGL rendering: viewport size must be greater than zero.");
        TBX_ASSERT(
            _current_frame_context->render_target != nullptr,
            "OpenGL rendering: frame context requires a render target framebuffer.");

        assign_frame_context();
        Pipeline::execute();
        reset_frame_context();
    }

    void OpenGlRenderPipeline::assign_frame_context()
    {
        TBX_ASSERT(
            _current_frame_context.has_value(),
            "OpenGL rendering: frame context must be set before assigning operations.");

        for (const auto& operation : get_operations())
        {
            auto* open_gl_operation = dynamic_cast<OpenGlRenderOperation*>(operation.get());
            TBX_ASSERT(
                open_gl_operation != nullptr,
                "OpenGL rendering: pipeline contains a non OpenGlRenderOperation instance.");
            open_gl_operation->set_frame_context(&*_current_frame_context);
        }
    }

    void OpenGlRenderPipeline::reset_frame_context()
    {
        for (const auto& operation : get_operations())
        {
            auto* open_gl_operation = dynamic_cast<OpenGlRenderOperation*>(operation.get());
            TBX_ASSERT(
                open_gl_operation != nullptr,
                "OpenGL rendering: pipeline contains a non OpenGlRenderOperation instance.");
            open_gl_operation->set_frame_context(nullptr);
        }

        _current_frame_context.reset();
    }
}
