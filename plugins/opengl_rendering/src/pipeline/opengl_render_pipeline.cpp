#include "opengl_render_pipeline.h"
#include "GeometryPassOperation.h"
#include "LightingPassOperation.h"
#include "ShadowPassOperation.h"
#include <any>

namespace opengl_rendering
{
    OpenGlRenderPipeline::OpenGlRenderPipeline(
        OpenGlResourceManager& resource_manager,
        tbx::JobSystem& job_system,
        OpenGlGBuffer& gbuffer)
        : _resource_manager(resource_manager)
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
    {
    }

    OpenGlRenderPipeline::~OpenGlRenderPipeline() noexcept
    {
        clear_operations();
    }

    void OpenGlRenderPipeline::execute(const std::any& payload)
    {
        _shadow_pass_operation->execute(payload);
        _geometry_pass_operation->execute(payload);
        _lighting_pass_operation->execute(payload);
        _transparent_pass_operation->execute(payload);
    }
}
