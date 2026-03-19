#include "opengl_render_pipeline.h"
#include "GeometryPassOperation.h"
#include "LightingPassOperation.h"
#include <any>

namespace opengl_rendering
{
    OpenGlRenderPipeline::OpenGlRenderPipeline(
        OpenGlResourceManager& resource_manager,
        OpenGlGBuffer& gbuffer)
        : _resource_manager(resource_manager)
        , _geometry_pass_operation(std::make_unique<GeometryPassOperation>(_resource_manager))
        , _lighting_pass_operation(std::make_unique<LightingPassOperation>(_resource_manager, gbuffer))
    {
    }

    OpenGlRenderPipeline::~OpenGlRenderPipeline() noexcept
    {
        clear_operations();
    }

    void OpenGlRenderPipeline::execute(const std::any& payload)
    {
        _geometry_pass_operation->execute(payload);
        _lighting_pass_operation->execute(payload);
    }
}
