#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/graphics/render_debug_view.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/systems/world/manager.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/components/post_processing.h"
#include "tbx/utils/result.h"
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace tbx
{
    struct PipelineResources;

    /// @brief
    /// Purpose: Executes the backend-facing render pipeline for one frame.
    /// @details
    /// Ownership: Owns GPU resource caches and borrows engine services. Thread Safety: Call on the
    /// render lane.
    class TBX_API RenderingPipeline final
    {
      public:
        RenderingPipeline(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager,
            std::weak_ptr<IWindowManager> window_manager,
            std::weak_ptr<WorldManager> world_manager);
        RenderingPipeline(
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
        /// Purpose: Runs transient allocation, frame graph construction, backend submission, and
        /// presentation of a world as seen by the given camera into the given target. Renders the
        /// active world unless world_override is non-null (e.g. the editor's isolated asset-preview
        /// world), in which case the override is rendered instead.
        Result execute(
            const GraphicsSettings& settings,
            const DeltaTime& delta_time,
            const CameraView& camera_view,
            const RenderTarget& output_target,
            const std::vector<std::shared_ptr<RenderPass>>& caller_passes = {},
            World* world_override = nullptr,
            uint64 frame_epoch = 0U);

        /// @brief
        /// Purpose: Invalidates cached GPU state affected by asset reloads.
        void reload();

        /// @brief
        /// Purpose: Registers a callback invoked on the render lane right before each present,
        /// while the back buffer still holds the finished frame. Pass an empty callback to clear.
        /// @details
        /// Thread Safety: Safe to call from any thread; the callback runs on the render lane and
        /// must not touch the pipeline.
        void set_pre_present_callback(
            std::function<
                void(IGraphicsBackend& backend, const RenderTarget& output_target, const Size& backbuffer_size)>
                callback);

        /// @brief
        /// Purpose: Replaces the pipeline's debug view (render-stage override + post-processing
        /// toggle, gated to cameras matching its tags). The default view is the normal frame.
        /// @details
        /// Thread Safety: Safe to call from any thread; each frame snapshots it under a lock.
        void set_debug_view(RenderDebugView debug_view);

      private:
        void invoke_pre_present_callback(
            IGraphicsBackend& backend,
            const RenderTarget& output_target,
            const Size& backbuffer_size);

        // Builds the ordered list of built-in frame passes (shadow, forward, tag mask, post) the
        // pipeline owns and runs each frame. Called once at construction.
        void build_passes();

        // Whether the shared per-frame setup produced a renderable frame, or why it did not.
        enum class FrameReadiness
        {
            Ready,      // The context is set up; the passes can run.
            ClearBlack, // No asset manager or no active world — clear to black and present.
            ClearSky,   // A valid world with nothing visible — clear to the sky color and present.
            Failed,     // A GPU setup step failed; out_failure carries the reason (fail the frame).
        };

        // Sets up the shared per-frame state every pass needs: advances the cache, resolves the world,
        // captures the world view, uploads this frame's transient buffers, builds the world bind group,
        // and fills the context's shared GPU handles (and the pipeline's transient frame state). Run
        // once per frame after the passes' prepare phase, before any pass executes.
        FrameReadiness prepare_frame(
            FramePassContext& context,
            const DeltaTime& delta_time,
            float light_cull_distance,
            World* world_override,
            Result& out_failure);

      private:
        std::weak_ptr<IGraphicsBackend> _backend = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::weak_ptr<IWindowManager> _window_manager = {};
        std::weak_ptr<WorldManager> _world_manager = {};
        std::unique_ptr<PipelineResources> _resources = {};
        // The pipeline's own built-in frame passes (ShadowPass, ForwardPass, PostPass — each in its own
        // file). Built once and run each frame, merged with the camera-matched caller passes the
        // Rendering service hands to execute(). Owned here as polymorphic RenderPass pointers.
        std::vector<std::unique_ptr<RenderPass>> _passes = {};
        std::mutex _pre_present_mutex = {};
        std::function<void(IGraphicsBackend&, const RenderTarget&, const Size&)> _pre_present_callback = {};
        // The editor's debug view (see set_debug_view), snapshotted per frame under its own lock —
        // written from the main thread while frames execute on the render lane.
        std::mutex _debug_view_mutex = {};
        RenderDebugView _debug_view = {};
        float _elapsed_time = 0.0F;
        uint64 _frame_index = 0U;
    };
}
