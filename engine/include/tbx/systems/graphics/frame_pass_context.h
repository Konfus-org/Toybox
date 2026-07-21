#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/post_processing.h"
#include <vector>
#include "tbx/types/size.h"
#include "tbx/types/typedefs.h"

namespace tbx
{
    /// @brief
    /// Purpose: The shared per-frame state a RenderPass operates on. The pipeline builds one each frame
    /// and threads it through every matching pass's prepare and execute, so passes share the frame's
    /// inputs without owning any of it.
    /// @details
    /// Holds only public, plugin-usable state. The engine-internal per-frame GPU state the built-in
    /// passes also need (the captured world view, the world bind group, the shadow sample group) lives
    /// on the pipeline's private Resources, not here, so this stays a clean public surface. Never
    /// outlives the frame. Thread Safety: Render-lane only.
    struct FramePassContext
    {
        IGraphicsBackend& backend;
        const CameraView& camera_view;
        const GraphicsSettings& settings;

        World* world = nullptr;
        Size output_size = {};
        uint64 frame_epoch = 0U;
        uint64 frame_index = 0U;
        uint32 stride = 0U;
        GpuId draw_args_buffer = INVALID_GPU_ID;
        GpuId uniforms_buffer = INVALID_GPU_ID;
        bool use_post = false;

        // The post effects contributed by this frame's caller passes (each pass camera-matched by
        // the Rendering service, so everything here applies to THIS camera); the post pass appends
        // them to the world's own stack and their entity-tag gates feed the tag mask.
        std::vector<PostProcessingEffect> extra_post_effects = {};

        // Frame GPU targets the post pass owns and publishes (in its prepare) for the other passes:
        // scene_color/scene_depth are the offscreen targets the forward pass renders into when post is
        // active; tag_mask is the silhouette the post chain samples. INVALID_GPU_ID when not on the
        // post path.
        GpuId scene_color = INVALID_GPU_ID;
        GpuId scene_depth = INVALID_GPU_ID;
        GpuId tag_mask = INVALID_GPU_ID;
    };
}
