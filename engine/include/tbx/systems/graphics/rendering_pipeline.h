#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/world/manager.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/utils/result.h"
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Executes the fixed GPU-driven render pipeline for one frame.
    /// @details
    /// Ownership: Owns GPU resource caches and borrows engine services. Thread Safety: Call on the
    /// render lane.
    class TBX_API RenderingPipeline final
    {
      public:
        explicit RenderingPipeline(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<IWindowManager> window_manager,
            std::weak_ptr<WorldManager> world_manager);
        explicit RenderingPipeline(
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<IWindowManager> window_manager,
            std::weak_ptr<WorldManager> world_manager);
        RenderingPipeline(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<IWindowManager> window_manager);
        ~RenderingPipeline();

      public:
        RenderingPipeline(const RenderingPipeline&) = delete;
        RenderingPipeline& operator=(const RenderingPipeline&) = delete;
        RenderingPipeline(RenderingPipeline&&) noexcept = delete;
        RenderingPipeline& operator=(RenderingPipeline&&) noexcept = delete;

      public:
        /// @brief
        /// Purpose: Runs frame setup, compute culling, indirect shadow/G-buffer draws, lighting
        /// resolve, and presentation.
        Result execute(
            IGraphicsBackend& backend,
            const GraphicsSettings& settings,
            const DeltaTime& delta_time);

        /// @brief
        /// Purpose: Invalidates cached GPU state affected by asset reloads.
        void reload();

      private:
        struct State;

      private:
        std::weak_ptr<IGraphicsBackend> _backend = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::weak_ptr<IWindowManager> _window_manager = {};
        std::weak_ptr<WorldManager> _world_manager = {};
        std::unique_ptr<State> _state = {};
        float _elapsed_time = 0.0F;
    };
}
