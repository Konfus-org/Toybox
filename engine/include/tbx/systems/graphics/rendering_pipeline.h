#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <memory>

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
            std::weak_ptr<IWindowManager> window_manager);
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
            const DeltaTime& delta_time);

      private:
        std::weak_ptr<EntityRegistry> _entity_registry = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::weak_ptr<IWindowManager> _window_manager = {};
        RenderingResourceManager _resource_manager;
        float _elapsed_time = 0;
        uint _frame_index = 0U;
    };
}
