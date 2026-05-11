#include "tbx/systems/graphics/pipeline/render_pipeline_config.h"
#include "tbx/systems/graphics/pipeline/commands/command_building_operations.h"
#include "tbx/systems/graphics/pipeline/context/frame_setup_operations.h"
#include "tbx/systems/graphics/pipeline/commands/pass_operations.h"

namespace tbx
{
    RenderPipelineConfig RenderPipelineConfig::standard()
    {
        auto config = RenderPipelineConfig();
        config.operations.push_back(std::make_unique<BuildRenderDataOperation>());
        config.operations.push_back(std::make_unique<CullNonVisibleRenderDataItemsOperation>());
        config.operations.push_back(std::make_unique<SelectCameraOperation>());
        config.operations.push_back(std::make_unique<BeginFrameOperation>());
        config.operations.push_back(std::make_unique<UpdateViewUniformsOperation>());
        config.operations.push_back(std::make_unique<ResolveVisibleObjectsOperation>());
        config.operations.push_back(std::make_unique<ResolveMaterialsOperation>());
        config.operations.push_back(std::make_unique<UploadMissingResourcesOperation>());
        config.operations.push_back(std::make_unique<BuildDirectionalShadowCommandsOperation>());
        config.operations.push_back(std::make_unique<BuildSkyboxCommandsOperation>());
        config.operations.push_back(std::make_unique<BuildOpaqueCommandsOperation>());
        config.operations.push_back(std::make_unique<BuildAlphaCutoutCommandsOperation>());
        config.operations.push_back(std::make_unique<BuildTransparentCommandsOperation>());
        config.operations.push_back(std::make_unique<ExecuteDirectionalShadowPassOperation>());
        config.operations.push_back(std::make_unique<ExecuteSkyboxPassOperation>());
        config.operations.push_back(std::make_unique<ExecuteOpaquePassOperation>());
        config.operations.push_back(std::make_unique<ExecuteAlphaCutoutPassOperation>());
        config.operations.push_back(std::make_unique<ExecuteTransparentPassOperation>());
        config.operations.push_back(std::make_unique<PresentOperation>());
        return config;
    }
}
