#include "tbx/systems/graphics/pipeline/render_pipeline_config.h"
#include "tbx/systems/graphics/pipeline/commands/command_building_operations.h"
#include "tbx/systems/graphics/pipeline/commands/pass_operations.h"
#include "tbx/systems/graphics/pipeline/context/frame_setup_operations.h"

namespace tbx
{
    RenderPipelineConfig RenderPipelineConfig::standard(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsResourceManager& resource_manager,
        std::weak_ptr<EntityRegistry> entity_registry,
        std::weak_ptr<IWindowManager> window_manager)
    {
        auto config = RenderPipelineConfig();
        config.operations.push_back(std::make_unique<BuildRenderDataOperation>(entity_registry));
        config.operations.push_back(
            std::make_unique<SelectCameraOperation>(entity_registry, window_manager));
        config.operations.push_back(std::make_unique<BeginFrameOperation>(backend));
        config.operations.push_back(
            std::make_unique<CullNonVisibleRenderDataItemsOperation>(resource_manager));
        config.operations.push_back(std::make_unique<UpdateViewUniformsOperation>(backend));
        config.operations.push_back(
            std::make_unique<BuildDirectionalShadowCommandsOperation>(backend, resource_manager));
        config.operations.push_back(
            std::make_unique<BuildSkyboxCommandsOperation>(backend, resource_manager));
        config.operations.push_back(
            std::make_unique<BuildOpaqueCommandsOperation>(backend, resource_manager));
        config.operations.push_back(std::make_unique<BuildAlphaCutoutCommandsOperation>());
        config.operations.push_back(std::make_unique<BuildTransparentCommandsOperation>());
        config.operations.push_back(
            std::make_unique<BuildFullscreenQuadResourcesOperation>(backend));
        config.operations.push_back(
            std::make_unique<BuildLightingCommandsOperation>(backend, resource_manager));
        config.operations.push_back(
            std::make_unique<BuildPostProcessCommandsOperation>(backend, resource_manager));
        config.operations.push_back(std::make_unique<ExecuteDirectionalShadowPassOperation>());
        config.operations.push_back(std::make_unique<ExecuteSkyboxPassOperation>());
        config.operations.push_back(std::make_unique<ExecuteOpaquePassOperation>());
        config.operations.push_back(std::make_unique<ExecuteAlphaCutoutPassOperation>());
        config.operations.push_back(std::make_unique<ExecuteTransparentPassOperation>());
        config.operations.push_back(std::make_unique<ExecuteLightingPassOperation>());
        config.operations.push_back(std::make_unique<ExecutePostProcessPassOperation>());
        config.operations.push_back(std::make_unique<PresentOperation>());
        return config;
    }
}

