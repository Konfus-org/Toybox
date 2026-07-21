#include "post_pass.h"
#include "../pipeline_internal.h"
#include "tbx/systems/debugging/macros.h"
#include <utility>

namespace tbx
{
    PostPass::PostPass(
        PipelineResources& resources,
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager)
        : RenderPass(PassType::Post)
        , _resources(resources)
        , _backend(backend)
        , _asset_manager(std::move(asset_manager))
        , _post(backend)
    {
    }

    Result PostPass::prepare(FramePassContext& context)
    {
        // The post pass owns its render targets: ensure the offscreen scene + tag-mask targets exist at
        // the current size before the forward pass renders into them (prepare runs before any execute),
        // then publish them on the context so the forward pass + the post chain can use them.
        if (!context.use_post)
            return Result::OK;
        if (auto result = _post.ensure_targets(context.output_size); !result)
            return result;
        context.scene_color = _post.get_scene_color();
        context.scene_depth = _post.get_scene_depth();
        context.tag_mask = _post.get_tag_mask();
        return Result::OK;
    }

    Result PostPass::execute(FramePassContext& context)
    {
        // Post-processing only runs on the offscreen path; the forward pass rendered straight to the
        // swapchain otherwise. Chains the enabled material-driven effects from the scene target to the
        // swapchain — a broken effect is skipped (warned), never the frame.
        if (!context.use_post || context.world == nullptr)
            return Result::OK;

        // Render the tag mask first when a tag-gated post effect is active this frame, so those effects
        // sample an up-to-date silhouette. Decided here (not via a context flag) so it stays an
        // automatic, internal detail of the post path: the mask exists only to serve tag-gated post
        // effects, which is exactly what a non-empty masked-tag set means.
        const bool needs_tag_mask =
            !PostProcessor::masked_tags(*context.world, context.extra_post_effects).empty();
        if (needs_tag_mask)
            if (auto result = render_tag_mask(context); !result)
                return result;

        // The asset manager is an external (non-GPU) service, so it is reached through the pipeline
        // rather than carried on the GPU-facing pass context.
        const auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return Result(false, "Post pass has no asset manager.");

        // Post effects come from the world's own PostProcessing components (scanned inside run) plus
        // the ones this camera's caller passes carry (the editor's selection outline rides a pass
        // gated on the editor-camera tag, so it never reaches game views).
        return _post.run(
            _resources.cache,
            *asset_manager,
            *context.world,
            context.output_size,
            context.uniforms_buffer,
            context.extra_post_effects);
    }

    Result PostPass::render_tag_mask(FramePassContext& context)
    {
        // Helper of the post pass (not a standalone pass): the caller renders this only on the post
        // path when a tag-gated effect is active. Draws the tagged entities' flat silhouettes into the
        // tag mask the effect samples.
        IGraphicsBackend& backend = context.backend;
        const WorldViewResult& view = *_resources.frame_view;
        FrameBuffers& frame = _resources.frame_buffers;
        const GpuResource& world_group = _resources.frame_world_group;
        const Size& output_size = context.output_size;
        const uint32 stride = context.stride;
        auto& post = _post;
        auto& cache = _resources.cache;

        // Always cleared so a stale mask never lingers; the silhouette is drawn only when something is
        // tagged.
        auto mask_pass = RenderPassDesc {
            .clear_color = Color(0.0F, 0.0F, 0.0F, 0.0F),
            .clear_flags = ClearFlags::COLOR};
        mask_pass.color_targets = {post.get_tag_mask()};
        mask_pass.viewport.dimensions = output_size;
        if (auto result = backend.begin_render_pass(mask_pass); !result)
            return result;

        if (!view.mask_draw_commands.empty())
        {
            const GpuId mask_args = frame.store(
                view.mask_draw_commands.data(),
                view.mask_draw_commands.size() * sizeof(GpuIndexedDrawCommand),
                BufferUsage::INDIRECT_ARGS);
            // Silhouette through walls: depth test off, two-sided, no blend (presence only).
            const auto mask_state = RasterState {
                .is_blending_enabled = false,
                .is_two_sided = true,
                .is_depth_test_enabled = false,
                .is_depth_write_enabled = false};
            const ShaderProgram mask_program = shader_program(
                MASK_VERTEX_SHADER_HANDLE,
                MASK_FRAGMENT_SHADER_HANDLE);
            const GpuId mask_pipeline =
                cache.add_pipeline(hash(mask_program, mask_state), mask_program, mask_state, true)
                    .value_or(INVALID_GPU_ID);
            if (mask_args != INVALID_GPU_ID && mask_pipeline != INVALID_GPU_ID)
            {
                if (auto result = backend.bind_raster_pipeline(mask_pipeline); !result)
                    return result;
                if (auto result = backend.bind_group(0U, world_group.get()); !result)
                    return result;
                if (auto result = backend.draw_indirect(
                        mask_args,
                        0U,
                        static_cast<uint32>(view.mask_draw_commands.size()),
                        stride);
                    !result)
                    return result;
            }
        }

        return backend.end_render_pass();
    }
}
