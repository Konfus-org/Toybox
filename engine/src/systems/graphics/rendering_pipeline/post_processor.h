#pragma once
#include "gpu_resource_cache.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/post_processing.h"
#include "tbx/types/size.h"
#include "tbx/utils/result.h"
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Runs the data-driven post-processing stage. Each enabled PostProcessingEffect is an
    /// ordinary material drawn as a fullscreen triangle that samples the rendered scene color
    /// (binding GPU_BINDING_SCENE_COLOR) and reads its own params/textures from the shared material
    /// table — so any effect is authored with the normal material/shader system, no bespoke plumbing.
    /// @details
    /// Owns (RAII) the offscreen scene color + a scratch color (RGBA16F) it ping-pongs between, a
    /// shared scene depth target the forward pass writes, and a tiny per-effect uniforms UBO. The
    /// forward pass renders into get_scene_color()/get_scene_depth(); run() then chains the effects,
    /// the last writing straight to the swapchain. A broken effect is skipped + warned, never blanking
    /// the frame. Targets are (re)created when the output size changes. Render-lane only.
    class PostProcessor final
    {
      public:
        explicit PostProcessor(std::weak_ptr<IGraphicsBackend> backend);
        ~PostProcessor() = default;

        PostProcessor(const PostProcessor&) = delete;
        PostProcessor& operator=(const PostProcessor&) = delete;

      public:
        /// @brief True when post-processing should run this frame: any enabled effect (from a world
        /// PostProcessing component or the caller-supplied extra_effects) whose tag gate is satisfied
        /// (≥1 matching entity). extra_effects lets a caller contribute effects from outside the world
        /// (e.g. the editor's overlay), processed exactly like the world's own.
        // Static: depends only on the world + effects, not on any post target state, so the pipeline can
        // ask it during prepare_frame without reaching into the post pass that owns the PostProcessor.
        static bool wants_post(World& world, const std::vector<PostProcessingEffect>& extra_effects = {});

        /// @brief The union of tag queries across all active tag-gated effects (world + extra). The
        /// pipeline renders entities matching any of these into the tag mask.
        static std::vector<std::string> masked_tags(
            World& world,
            const std::vector<PostProcessingEffect>& extra_effects = {});

        /// @brief (Re)creates the offscreen targets when the size changes; idempotent otherwise.
        Result ensure_targets(const Size& size);

        /// @brief The scene color target the forward pass renders into (sampled by the first effect).
        GpuId get_scene_color() const;
        /// @brief The scene depth target paired with get_scene_color() for the forward pass.
        GpuId get_scene_depth() const;
        /// @brief The tag mask target the pipeline draws tagged silhouettes into (bound to effects at
        /// GPU_BINDING_TAG_MASK).
        GpuId get_tag_mask() const;

        /// @brief Chains the enabled effects scene_color -> ... -> swapchain. uniforms_buffer is this
        /// frame's scene uniforms (binding GPU_BINDING_UNIFORMS). extra_effects are appended after the
        /// world's own stack. Never fatal on a single bad effect.
        Result run(
            GpuResourceCache& cache,
            AssetManager& assets,
            World& world,
            const Size& output_size,
            GpuId uniforms_buffer,
            const std::vector<PostProcessingEffect>& extra_effects = {});

      private:
        std::weak_ptr<IGraphicsBackend> _backend = {};
        Size _size = {0U, 0U};
        GpuResource _scene_color = {};
        GpuResource _scratch = {};
        GpuResource _scene_depth = {};
        GpuResource _tag_mask = {};
        GpuResource _post_uniforms = {};
    };
}
