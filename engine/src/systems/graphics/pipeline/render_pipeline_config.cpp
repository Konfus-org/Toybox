#include "tbx/systems/graphics/pipeline/render_pipeline_config.h"
#include "tbx/systems/graphics/pipeline/commands/alpha_cutout_pass_operation.h"
#include "tbx/systems/graphics/pipeline/commands/directional_shadow_pass_operation.h"
#include "tbx/systems/graphics/pipeline/commands/lighting_pass_operation.h"
#include "tbx/systems/graphics/pipeline/commands/opaque_pass_operation.h"
#include "tbx/systems/graphics/pipeline/commands/post_process_pass_operation.h"
#include "tbx/systems/graphics/pipeline/commands/skybox_pass_operation.h"
#include "tbx/systems/graphics/pipeline/commands/transparent_pass_operation.h"

namespace tbx
{
    RenderPipelineConfig RenderPipelineConfig::standard(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsResourceManager& resource_manager)
    {
        auto config = RenderPipelineConfig();
        config.operations.push_back(
            std::make_unique<DirectionalShadowPassOperation>(backend, resource_manager));
        config.operations.push_back(
            std::make_unique<SkyboxPassOperation>(backend, resource_manager));
        config.operations.push_back(
            std::make_unique<OpaquePassOperation>(backend, resource_manager));
        config.operations.push_back(std::make_unique<AlphaCutoutPassOperation>());
        config.operations.push_back(
            std::make_unique<LightingPassOperation>(backend, resource_manager));
        config.operations.push_back(std::make_unique<TransparentPassOperation>());
        config.operations.push_back(
            std::make_unique<PostProcessPassOperation>(backend, resource_manager));
        return config;
    }
}

