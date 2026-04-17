#include "opengl_render_pipeline.h"
#include "GeometryPassOperation.h"
#include "LightingPassOperation.h"
#include "RenderPipelineFailure.h"
#include "ShadowPassOperation.h"
#include "opengl_resources/opengl_resource.h"
#include "tbx/debugging/macros.h"
#include <any>
#include <glad/glad.h>

namespace opengl_rendering
{
    OpenGlRenderPipeline::OpenGlRenderPipeline(
        OpenGlResourceManager& resource_manager,
        tbx::JobSystem& job_system,
        OpenGlGBuffer& gbuffer)
        : _resource_manager(resource_manager)
        , _gbuffer(gbuffer)
        , _shadow_pass_operation(std::make_unique<ShadowPassOperation>(_resource_manager))
        , _geometry_pass_operation(std::make_unique<GeometryPassOperation>(_resource_manager))
        , _lighting_pass_operation(
              std::make_unique<LightingPassOperation>(
                  _resource_manager,
                  job_system,
                  gbuffer,
                  *_shadow_pass_operation))
        , _transparent_pass_operation(
              std::make_unique<TransparentPassOperation>(_resource_manager, gbuffer))
        , _post_processing_pass_operation(
              std::make_unique<PostProcessingPassOperation>(_resource_manager, gbuffer))
    {
    }

    OpenGlRenderPipeline::~OpenGlRenderPipeline() noexcept
    {
        clear_operations();
    }

    void OpenGlRenderPipeline::execute(const std::any& payload)
    {
        clear_render_pipeline_failure();
        _shadow_pass_operation->execute(payload);
        _gbuffer.prepare_geometry_pass();
        _geometry_pass_operation->execute(payload);
        _lighting_pass_operation->execute(payload);
        _transparent_pass_operation->execute(payload);
        _post_processing_pass_operation->execute(payload);
        if (!has_render_pipeline_failure())
        {
            _has_reported_pipeline_failure = false;
            return;
        }

        if (!_has_reported_pipeline_failure)
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: one or more render passes failed without producing a usable "
                "frame. Rendering magenta fallback frame.");
            _has_reported_pipeline_failure = true;
        }
        render_magenta_failure_frame(_gbuffer);
    }

    void OpenGlRenderPipeline::render_magenta_failure_frame(OpenGlGBuffer& gbuffer)
    {
        auto gbuffer_scope = OpenGlResourceScope(gbuffer);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glClearColor(1.0F, 0.0F, 1.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
    }
}
