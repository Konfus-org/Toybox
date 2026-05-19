#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/frame_data_factory.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <future>
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Orchestrates the per-frame render loop and submits frame work to the render lane.
    /// @details
    /// Ownership: Owns the resource manager and frame-data factory. Borrows services.
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

      private:
        Result execute_draw_command(IGraphicsBackend& backend, const GraphicsDrawCommand& command) const;
        Result execute_draw_command(
            IGraphicsBackend& backend,
            const GraphicsIndexedDrawCommand& command) const;
        void initialize(const GraphicsSettings& settings);
        void render_frame();

        void wait_for_initialization() noexcept;
        void wait_for_render_frame() noexcept;

        std::weak_ptr<ThreadManager> _thread_manager;
        std::weak_ptr<IGraphicsBackend> _backend;
        std::weak_ptr<IWindowManager> _window_manager;
        Size _configured_resolution = {};
        std::unique_ptr<GraphicsResourceManager> _resource_manager = nullptr;
        FrameDataFactory _frame_data_factory;
        uint64 _frame_index = 0U;

        Result _initialization_result = {};
        std::future<void> _initialization_future = {};
        std::future<void> _render_future = {};
    };
}
