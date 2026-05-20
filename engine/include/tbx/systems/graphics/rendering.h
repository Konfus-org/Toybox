#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/draw_command_executor.h"
#include "tbx/systems/graphics/rendering_pass_factory.h"
#include "tbx/systems/graphics/resource_tracker.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <future>
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Orchestrates the per-frame render loop and submits frame work to the render lane.
    /// @details
    /// Ownership: Owns the frame pipeline factory, executor, and resource tracker. Borrows
    /// services.
    /// Thread Safety: Not inherently thread-safe; lifecycle and backend work are submitted to the
    /// dedicated render lane.
    class TBX_API Rendering
    {
      public:
        Rendering(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<EntityRegistry> entity_registry,
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<ThreadManager> thread_manager,
            std::weak_ptr<IWindowManager> window_manager,
            const GraphicsSettings& settings);
        ~Rendering() noexcept;

      public:
        Rendering(const Rendering&) = delete;
        Rendering& operator=(const Rendering&) = delete;
        Rendering(Rendering&&) noexcept = delete;
        Rendering& operator=(Rendering&&) noexcept = delete;

      public:
        void render();
        /// @brief
        /// Purpose: Blocks until any previously dispatched render frame has finished.
        /// @details
        /// Thread Safety: Call from the main thread before mutating scene data shared with the
        /// renderer.
        void wait_for_pending_frame() noexcept;

      private:
        void initialize(const GraphicsSettings& settings);

        void render_frame();
        void end_frame(IGraphicsBackend& backend, DeltaTime delta_time);
        uint unload_expired_resources(IGraphicsBackend& backend, float max_time_alive_seconds);

        void wait_for_initialization() noexcept;
        void wait_for_render_frame() noexcept;

        std::weak_ptr<ThreadManager> _thread_manager;
        std::weak_ptr<IGraphicsBackend> _backend;
        RenderingResourceTracker _resource_tracker = {};
        RenderingPassFactory _pass_factory;
        DrawCommandExecutor _draw_command_executor = {};
        DeltaTimer _frame_timer = {};
        uint64 _frame_index = 0U;

        Result _initialization_result = {};
        std::future<void> _initialization_future = {};
        std::future<void> _render_future = {};
    };
}
