#pragma once
#include "tbx/systems/graphics/frame_pass_context.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: When a pass runs within the frame, relative to the others. The pipeline runs all
    /// passes of one type before the next: scene geometry, then the post-processing composite, then
    /// overlays drawn on top of the finished image. Passes keep their relative order within a type.
    enum class PassType
    {
        Scene,   // Geometry into the scene (shadow maps, the forward pass).
        Post,    // Post-processing that composites the scene to the swapchain.
        Overlay, // Drawn on top of the fully composited frame (e.g. editor gizmos).
    };

    /// @brief
    /// Purpose: A render pass — one self-contained stage of the frame. The pipeline's own built-in
    /// stages (shadow, forward, post) and caller-supplied passes (e.g. the editor's gizmo overlay) all
    /// derive from this: each owns its GPU resources, prepares them in `prepare`, and does its
    /// draw/upload work in `execute`, running only for cameras matching its tag gate at the point its
    /// `type` dictates. Subclassing keeps the pipeline a generic pass runner that knows nothing about
    /// what any individual pass does.
    /// @details
    /// Ownership: Held by `std::unique_ptr` (pipeline built-ins) or `std::shared_ptr` (caller passes,
    /// so an in-flight frame keeps the pass alive across the render lane). Thread Safety: Register /
    /// remove from the main thread; prepare/execute run on the render lane against the FramePassContext.
    /// A caller pass defined in a plugin must be removed before the plugin unloads (its vtable lives in
    /// the plugin module).
    class TBX_API RenderPass
    {
      public:
        explicit RenderPass(PassType type = PassType::Scene, std::vector<std::string> camera_tags = {})
            : type(type)
            , camera_tags(std::move(camera_tags))
        {
        }
        virtual ~RenderPass() = default;

        RenderPass(const RenderPass&) = delete;
        RenderPass& operator=(const RenderPass&) = delete;

        /// @brief Prepares the pass's GPU resources for this frame (allocate/resize textures, upload
        /// buffers, build pipelines). Runs for every matching pass after the scene is captured and
        /// before any pass executes. Default: nothing. A failing Result fails the frame.
        virtual Result prepare(FramePassContext&)
        {
            return Result::OK;
        }

        /// @brief The pass's draw/upload work, run in pass order (by type) after every prepare. Default:
        /// nothing. A failing Result fails the frame.
        virtual Result execute(FramePassContext&)
        {
            return Result::OK;
        }

        /// @brief When this pass runs relative to the others (scene → post → overlay).
        PassType type = PassType::Scene;

        /// @brief Gates the pass on the rendering camera — it contributes only when the camera carries
        /// at least one of these tags. Empty means the pass applies to every camera.
        std::vector<std::string> camera_tags = {};

        /// @brief Post effects this pass contributes to the frame's post chain, run beside the world's
        /// own PostProcessing effects (an effect's entity-tag gate works exactly as authored, feeding
        /// the tag mask — how the editor's selection outline triggers purely off the editor.selected
        /// tag). Contributed only for cameras matching the pass's tag gate, like prepare/execute.
        std::vector<PostProcessingEffect> post_effects = {};
    };

    /// @brief
    /// Purpose: A RenderPass whose prepare/execute are supplied as callbacks rather than overridden in
    /// a subclass — for simple passes that hold no resources of their own (e.g. the editor gizmo
    /// overlay, which just draws an existing service). A resource-owning pass should subclass RenderPass
    /// directly so it can own its GPU state.
    class TBX_API CallbackRenderPass final : public RenderPass
    {
      public:
        using Callback = std::function<Result(FramePassContext& context)>;

        CallbackRenderPass(
            PassType type,
            std::vector<std::string> camera_tags,
            Callback prepare,
            Callback execute)
            : RenderPass(type, std::move(camera_tags))
            , _prepare(std::move(prepare))
            , _execute(std::move(execute))
        {
        }

        Result prepare(FramePassContext& context) override
        {
            return _prepare ? _prepare(context) : Result::OK;
        }

        Result execute(FramePassContext& context) override
        {
            return _execute ? _execute(context) : Result::OK;
        }

      private:
        Callback _prepare = {};
        Callback _execute = {};
    };
}
