#include "opengl_render_pipeline.h"
#include "opengl_resources/opengl_resource.h"
#include "tbx/debugging/macros.h"
#include <glad/glad.h>
#include <utility>

namespace tbx::plugins
{
    template <typename TResource>
    static void trim_resource_cache_entries(
        std::unordered_map<Uuid, std::shared_ptr<TResource>>& cache,
        const std::unordered_set<Uuid>& live_ids)
    {
        for (auto iterator = cache.begin(); iterator != cache.end();)
        {
            if (!live_ids.contains(iterator->first))
            {
                iterator = cache.erase(iterator);
                continue;
            }

            ++iterator;
        }
    }

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
        clear_resource_caches();
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

    void OpenGlRenderPipeline::clear_resource_caches()
    {
        _meshes_by_id.clear();
        _textures_by_id.clear();
        _shader_programs_by_id.clear();
        _materials_by_id.clear();
        _models_by_id.clear();
    }

    void OpenGlRenderPipeline::trim_resource_caches(
        const std::unordered_set<Uuid>& live_mesh_ids,
        const std::unordered_set<Uuid>& live_texture_ids,
        const std::unordered_set<Uuid>& live_shader_program_ids,
        const std::unordered_set<Uuid>& live_material_ids,
        const std::unordered_set<Uuid>& live_model_ids)
    {
        trim_resource_cache_entries(_meshes_by_id, live_mesh_ids);
        trim_resource_cache_entries(_textures_by_id, live_texture_ids);
        trim_resource_cache_entries(_shader_programs_by_id, live_shader_program_ids);
        trim_resource_cache_entries(_materials_by_id, live_material_ids);
        trim_resource_cache_entries(_models_by_id, live_model_ids);
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
