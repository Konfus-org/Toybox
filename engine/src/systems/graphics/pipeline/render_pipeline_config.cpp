#include "tbx/systems/graphics/pipeline/render_pipeline_config.h"
#include "tbx/systems/graphics/pipeline/opaque_scene_operation.h"
#include "tbx/systems/graphics/pipeline/skybox_operation.h"
#include "tbx/systems/graphics/pipeline/view_uniform_operation.h"

namespace tbx
{
    RenderPipelineConfig RenderPipelineConfig::standard()
    {
        auto config = RenderPipelineConfig();
        config.operations.push_back(std::make_unique<ViewUniformOperation>());
        config.operations.push_back(std::make_unique<SkyboxOperation>());
        config.operations.push_back(std::make_unique<OpaqueSceneOperation>());
        return config;
    }
}
