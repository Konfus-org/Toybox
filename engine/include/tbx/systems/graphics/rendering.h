#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/reload_queue.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/world/manager.h"
#include "tbx/systems/graphics/rendering_pipeline.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/time/delta_time.h"
#include <future>

namespace tbx
{
    // TODO: make shadow cascades fully configurable from graphics settings and make shadows render
    // really far by default, but far shadows should use a super low resolution
    /// @brief
    /// Purpose: Orchestrates the per-frame render loop and submits frame work to the render lane.
    /// @details
    /// Ownership: Owns the rendering pipeline and borrows services.
    /// Thread Safety: Not inherently thread-safe; lifecycle and backend work are submitted to the
    /// dedicated render lane.
    class TBX_API Rendering
    {
      public:
        Rendering(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<ThreadManager> thread_manager,
            std::weak_ptr<IWindowManager> window_manager,
            std::weak_ptr<WorldManager> world_manager = {},
            std::weak_ptr<AssetReloadQueue> reload_queue = {});
        ~Rendering() noexcept;

      public:
        Rendering(const Rendering&) = delete;
        Rendering& operator=(const Rendering&) = delete;
        Rendering(Rendering&&) noexcept = delete;
        Rendering& operator=(Rendering&&) noexcept = delete;

      public:
        /// @brief
        /// Purpose: Renders the world for the current frame.
        /// @details
        /// Thread Safety: Call from the message dispatch thread while no frame is pending.
        void render(const DeltaTime& delta_time, const GraphicsSettings& settings);

        /// @brief
        /// Purpose: Blocks until any previously dispatched render frame has finished.
        /// @details
        /// Thread Safety: Call from the main thread before mutating scene data shared with the
        /// renderer.
        void wait_for_pending_frame() noexcept;

      private:
        void on_asset_reload(const AssetReloadContext& context);
        void render_frame(const DeltaTime& delta_time, const GraphicsSettings& settings);
        void wait_for_render_frame() noexcept;

      private:
        std::weak_ptr<ThreadManager> _thread_manager;
        std::weak_ptr<AssetReloadQueue> _reload_queue;
        std::weak_ptr<IGraphicsBackend> _backend;
        std::weak_ptr<IWindowManager> _window_manager;
        RenderingPipeline _pipeline;

        std::future<void> _render_future = {};
        Uuid _asset_reload_handler = {};
    };
}
