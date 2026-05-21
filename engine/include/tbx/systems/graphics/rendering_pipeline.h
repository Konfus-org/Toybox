#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/draw_command_executor.h"
#include "tbx/systems/graphics/draw_command_factory.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/systems/graphics/resource_tracker.h"
#include "tbx/systems/graphics/resource_uploader.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Sets up, draws, and unloads one rendering frame.
    /// @details
    /// Ownership: Owns command construction, frame shader data, upload helpers, resource tracking,
    /// and unload policy.
    /// Thread Safety: Call on the render lane.
    class TBX_API RenderingPipeline final
    {
      public:
        RenderingPipeline(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<EntityRegistry> entity_registry,
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<IWindowManager> window_manager,
            const GraphicsSettings& settings);
        ~RenderingPipeline() = default;

      public:
        RenderingPipeline(const RenderingPipeline&) = delete;
        RenderingPipeline& operator=(const RenderingPipeline&) = delete;
        RenderingPipeline(RenderingPipeline&&) noexcept = delete;
        RenderingPipeline& operator=(RenderingPipeline&&) noexcept = delete;

      public:
        /// @brief
        /// Purpose: Executes the full setup, draw, and unload sequence for one frame.
        Result execute(
            IGraphicsBackend& backend,
            const GraphicsSettings& settings,
            const DeltaTime& delta_time,
            const uint frame_index);

      private:
        std::weak_ptr<EntityRegistry> _entity_registry = {};
        std::weak_ptr<IWindowManager> _window_manager = {};
        RenderingResourceTracker _resource_tracker = {};
        RenderingDrawCommandFactory _draw_command_factory = {};
        DrawCommandExecutor _draw_command_executor = {};
        ResourceUploader _resource_uploader;
        float _elapsed_time = 0;
    };
}
