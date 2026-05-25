#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/rendering_pipeline.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/messaging/message.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <future>
#include <memory>
#include <mutex>

namespace tbx
{
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
        /// @brief
        /// Purpose: Renders the world for the current frame.
        /// @details
        /// Thread Safety: Call from the message dispatch thread while no frame is pending.
        void render(const DeltaTime& delta_time);

        /// @brief
        /// Purpose: Applies graphics settings change messages to the renderer's cached settings.
        /// @details
        /// Thread Safety: Call from the message dispatch thread while no frame is pending.
        void receive_message(Message& msg);

        /// @brief
        /// Purpose: Blocks until any previously dispatched render frame has finished.
        /// @details
        /// Thread Safety: Call from the main thread before mutating scene data shared with the
        /// renderer.
        void wait_for_pending_frame() noexcept;

      private:
        void render_frame(const DeltaTime& delta_time);

        void wait_for_render_frame() noexcept;

        std::weak_ptr<ThreadManager> _thread_manager;
        std::weak_ptr<IGraphicsBackend> _backend;
        std::weak_ptr<IWindowManager> _window_manager;
        RenderingPipeline _pipeline;

        std::mutex _settings_mutex = {};
        GraphicsSettings _settings;

        std::future<void> _render_future = {};
    };
}
