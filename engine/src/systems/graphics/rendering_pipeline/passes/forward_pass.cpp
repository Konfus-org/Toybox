#include "forward_pass.h"
#include "../pipeline_internal.h"
#include "tbx/systems/debugging/macros.h"
#include <utility>

namespace tbx
{
    ForwardPass::ForwardPass(PipelineResources& resources, std::weak_ptr<IGraphicsBackend> backend)
        : RenderPass(PassType::Scene)
        , _resources(resources)
        , _backend(std::move(backend))
    {
    }

    Result ForwardPass::execute(FramePassContext& context)
    {
        IGraphicsBackend& backend = context.backend;
        const WorldViewResult& view = *_resources.frame_view;
        const GpuResource& world_group = _resources.frame_world_group;
        const GpuResource& shadow_sample_group = _resources.frame_shadow_sample_group;
        const GpuId draw_args_buffer = context.draw_args_buffer;
        const Size& output_size = context.output_size;
        const uint32 stride = context.stride;
        const bool use_post = context.use_post;
        const uint64 frame_index = context.frame_index;

        // Each material shades itself against the light list. When post-processing is active the pass
        // renders into the offscreen scene target (owned by the post pass, published on the context)
        // the effect chain consumes; otherwise straight to the swapchain.
        auto pass = RenderPassDesc {
            .clear_color = SKY_COLOR,
            .clear_depth = 1.0F,
            .clear_flags = ClearFlags::COLOR_DEPTH};
        pass.viewport.dimensions = output_size;
        if (use_post)
        {
            pass.color_targets = {context.scene_color};
            pass.depth_stencil_target = context.scene_depth;
        }
        if (auto result = backend.begin_render_pass(pass); !result)
            return result;

        uint64 command_offset = 0U;
        const bool diag = frame_index == 2U;
        if (diag)
            TBX_TRACE_INFO(
                "DRAW INFO: buckets={} instances={} lights={}",
                view.bucket_pipelines.size(),
                view.instances.size(),
                view.lights.size());
        for (uint32 bucket = 0U; bucket < view.bucket_pipelines.size(); ++bucket)
        {
            const uint32 count = view.bucket_command_counts[bucket];
            const GpuId pipeline = view.bucket_pipelines[bucket];
            if (diag)
                TBX_TRACE_INFO(
                    "DRAW BUCKET INFO {}: pipeline={} count={}",
                    bucket,
                    static_cast<uint64>(pipeline),
                    count);
            if (count != 0U && pipeline != INVALID_GPU_ID)
            {
                if (auto result = backend.bind_raster_pipeline(pipeline); !result)
                    return result;
                if (auto result = backend.bind_group(0U, world_group.get()); !result)
                    return result;
                if (shadow_sample_group.is_valid())
                    if (auto result = backend.bind_group(1U, shadow_sample_group.get()); !result)
                        return result;
                if (auto result = backend.draw_indirect(
                        draw_args_buffer,
                        command_offset * stride,
                        count,
                        stride);
                    !result)
                    return result;
            }
            command_offset += count;
        }

        return backend.end_render_pass();
    }
}
