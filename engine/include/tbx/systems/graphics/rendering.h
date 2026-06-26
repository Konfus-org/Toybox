#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/world/manager.h"
#include "tbx/systems/graphics/external_camera.h"
#include "tbx/systems/graphics/render_pass.h"
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
#include <tuple>
#include <unordered_map>
#include <utility>
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
            const std::shared_ptr<World>& world_override = {});

        /// @brief
        /// Purpose: Registers a render pass layered onto every frame whose camera matches the pass's
        /// tag gate (gizmo overlays, extra post effects). Returns an id used to remove it later. The
        /// renderer's render() entry point is otherwise unaware of editor/tooling concerns — they all
        /// arrive through registered passes. Pass-less rendering (a shipped game) is unchanged.
        /// @details
        /// Thread Safety: Call from the main thread; the renderer snapshots matching passes per frame.
        /// Held as a shared_ptr so an in-flight frame keeps the pass alive across the render lane; a
        /// pass defined in a plugin must be removed before the plugin unloads (its vtable lives there).
        Uuid add_render_pass(std::shared_ptr<RenderPass> pass);

        /// @brief
        /// Purpose: Removes a previously registered render pass. A no-op for an unknown id.
        /// @details
        /// Thread Safety: Call from the main thread.
        void remove_render_pass(const Uuid& id);

        /// @brief
        /// Purpose: Registers an external camera (an editor viewport or asset preview) the engine
        /// renders each frame after the active world's cameras — a camera that is not an entity in the
        /// active world. Returns an id used to update or remove it.
        /// @details
        /// Thread Safety: Call from the main thread. The owner mutates the camera between frames via
        /// update_external_camera; the engine snapshots it for the render lane in render_external_cameras.
        ExternalCameraId register_external_camera(ExternalCamera camera);

        /// @brief
        /// Purpose: Replaces a registered external camera's snapshot (new pose, target, or world). The
        /// owner calls this each frame before the engine renders. A no-op for an unknown id.
        /// @details
        /// Thread Safety: Call from the main thread.
        void update_external_camera(const ExternalCameraId& id, ExternalCamera camera);

        /// @brief
        /// Purpose: Removes a registered external camera (and drops the engine's reference to its
        /// override world). A no-op for an unknown id.
        /// @details
        /// Thread Safety: Call from the main thread.
        void unregister_external_camera(const ExternalCameraId& id);

        /// @brief
        /// Purpose: Renders every registered external camera into its own target. The app loop calls
        /// this once per frame after the active world's cameras; it snapshots the registry and dispatches
        /// one render() per external camera through the same path as world cameras.
        /// @details
        /// Thread Safety: Call from the main thread, while no frame is pending (same contract as render).
        void render_external_cameras(const DeltaTime& delta_time, const GraphicsSettings& settings);

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
            std::vector<std::shared_ptr<RenderPass>> passes,
            std::shared_ptr<World> world_override,
            uint64 frame_epoch);
        void wait_for_render_frame() noexcept;
        void evict_stale_lanes();

      private:
        std::weak_ptr<ThreadManager> _thread_manager;
        std::weak_ptr<IMessageCoordinator> _message_coordinator;
        std::weak_ptr<IGraphicsBackend> _backend;
        std::weak_ptr<IWindowManager> _window_manager;
        RenderingPipeline _pipeline;

        std::unordered_map<uint64, RenderLane> _render_lanes = {};
        uint64 _lane_touch_counter = 0U;
        std::mutex _render_lanes_mutex = {};
        Uuid _asset_reload_handler = {};

        // Advanced once per application frame (in wait_for_pending_frame, called at frame start). All
        // of a frame's per-view render() calls carry the same epoch, letting the pipeline run
        // view-independent per-frame work (the camera-independent local-light shadow atlas) once and
        // reuse it across the frame's views instead of regenerating it per view.
        uint64 _frame_epoch = 0U;

        // Registered render passes (e.g. the editor gizmo overlay), each gated on the camera's tags.
        // Snapshotted under the mutex per render() so registration can run on the main thread while
        // frames dispatch; the vector preserves registration order for deterministic sequencing.
        std::vector<std::pair<Uuid, std::shared_ptr<RenderPass>>> _render_passes = {};
        std::mutex _render_passes_mutex = {};

        // External cameras (editor viewports / asset previews) the engine renders that are not entities
        // in the active world. Snapshotted under the mutex per frame so the owner can register/update on
        // the main thread while frames dispatch; render_external_cameras copies them out and renders
        // outside the lock.
        std::vector<std::pair<ExternalCameraId, ExternalCamera>> _external_cameras = {};
        std::mutex _external_cameras_mutex = {};
    };
}
