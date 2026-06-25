#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/world/manager.h"
#include "tbx/systems/graphics/gizmos.h"
#include "tbx/systems/graphics/rendering_pipeline.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/components/post_processing.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
#include <filesystem>
#include <functional>
#include <future>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Tracks the most recent in-flight frame for one render target so each target's
    /// completion is observed independently while all GL work still funnels to the single
    /// render lane.
    struct RenderLane
    {
        std::future<void> frame = {};
        uint64 last_touch = 0U;
    };

    // Shadow cascades are currently fixed in the renderer until graphics settings owns that policy.
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
            std::weak_ptr<IMessageCoordinator> message_coordinator = {},
            std::weak_ptr<Gizmos> gizmos = {},
            Handle render_pipeline_script = {});
        ~Rendering() noexcept;

      public:
        Rendering(const Rendering&) = delete;
        Rendering& operator=(const Rendering&) = delete;
        Rendering(Rendering&&) noexcept = delete;
        Rendering& operator=(Rendering&&) noexcept = delete;

      public:
        /// @brief
        /// Purpose: Renders a world from the given camera view into the given target, which
        /// may be a window or an in-memory render texture. Renders the active world unless
        /// world_override is given (e.g. the editor's isolated asset-preview world), in which case
        /// the override is rendered instead. The caller must keep the override alive across the
        /// dispatch; the render lambda captures the shared_ptr to guarantee that.
        /// @details
        /// Thread Safety: Call from the message dispatch thread while no frame is pending.
        void render(
            const DeltaTime& delta_time,
            const GraphicsSettings& settings,
            const CameraView& camera_view,
            const RenderTarget& output_target,
            const std::vector<PostProcessingEffect>& extra_post_effects = {},
            const std::shared_ptr<World>& world_override = {});

        /// @brief
        /// Purpose: Registers a callback invoked on the render lane right before each present,
        /// while the back buffer still holds the finished frame. Pass an empty callback to clear.
        /// @details
        /// Thread Safety: Safe to call from any thread.
        void set_pre_present_callback(
            std::function<
                void(IGraphicsBackend& backend, const RenderTarget& output_target, const Size& backbuffer_size)>
                callback);

        /// @brief
        /// Purpose: Captures the next fully rendered frame to a 32-bit BMP at path via GPU
        /// readback, then invokes on_complete(succeeded) exactly once. A few warm-up frames are
        /// skipped first so the world's synchronous first-frame asset load finishes before the
        /// capture, and the readback is retried until it becomes ready or a frame budget elapses.
        /// @details
        /// Thread Safety: Safe to call from any thread; the capture runs on the render lane.
        /// This is the reliable way to validate rendering headlessly — window captures
        /// (GDI/PrintWindow) return black for hardware OpenGL surfaces regardless of content.
        void capture_screenshot(
            std::filesystem::path path,
            std::function<void(bool succeeded)> on_complete = {});

        /// @brief
        /// Purpose: Blocks until any previously dispatched render frame has finished.
        /// @details
        /// Thread Safety: Call from the main thread before mutating world data shared with the
        /// renderer.
        void wait_for_pending_frame() noexcept;

      private:
        void on_asset_reloaded(const AssetReloadedEvent& event);
        void render_frame(
            const DeltaTime& delta_time,
            const GraphicsSettings& settings,
            const CameraView& camera_view,
            const RenderTarget& output_target,
            std::shared_ptr<Gizmos> gizmos,
            const std::vector<PostProcessingEffect>& extra_post_effects,
            std::shared_ptr<World> world_override);
        void wait_for_render_frame() noexcept;
        void evict_stale_lanes();

      private:
        std::weak_ptr<ThreadManager> _thread_manager;
        std::weak_ptr<IMessageCoordinator> _message_coordinator;
        std::weak_ptr<IGraphicsBackend> _backend;
        std::weak_ptr<IWindowManager> _window_manager;
        std::weak_ptr<Gizmos> _gizmos;
        RenderingPipeline _pipeline;

        std::unordered_map<uint64, RenderLane> _render_lanes = {};
        uint64 _lane_touch_counter = 0U;
        std::mutex _render_lanes_mutex = {};
        Uuid _asset_reload_handler = {};
    };
}
