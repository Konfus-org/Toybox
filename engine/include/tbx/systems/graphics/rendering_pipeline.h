#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/graphics/gizmos.h"
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
        /// presentation of the active world as seen by the given camera into the given target.
        Result execute(
            const GraphicsSettings& settings,
            const DeltaTime& delta_time,
            const CameraView& camera_view,
            const RenderTarget& output_target,
            Gizmos* gizmos = nullptr,
            const std::vector<PostProcessingEffect>& extra_post_effects = {});

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

      private:
        struct Resources;

      private:
        void invoke_pre_present_callback(
            IGraphicsBackend& backend,
            const RenderTarget& output_target,
            const Size& backbuffer_size);

      private:
        std::weak_ptr<IGraphicsBackend> _backend = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::weak_ptr<IWindowManager> _window_manager = {};
        std::weak_ptr<WorldManager> _world_manager = {};
        std::unique_ptr<Resources> _resources = {};
        std::mutex _pre_present_mutex = {};
        std::function<void(IGraphicsBackend&, const RenderTarget&, const Size&)> _pre_present_callback = {};
        float _elapsed_time = 0.0F;
        uint64 _frame_index = 0U;
    };
}
