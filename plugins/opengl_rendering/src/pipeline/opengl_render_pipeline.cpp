#include "opengl_render_pipeline.h"
#include "GeometryPassOperation.h"
#include <any>

namespace opengl_rendering
{
    OpenGlRenderPipeline::OpenGlRenderPipeline(const OpenGlResourceManager& resource_manager)
        : _resource_manager(resource_manager)
        , _geometry_pass_operation(std::make_unique<GeometryPassOperation>(_resource_manager))
    {
    }

    OpenGlRenderPipeline::~OpenGlRenderPipeline() noexcept
    {
        clear_operations();
    }

    void OpenGlRenderPipeline::execute(const std::any& payload)
    {
        _geometry_pass_operation->execute(payload);
    }
}
