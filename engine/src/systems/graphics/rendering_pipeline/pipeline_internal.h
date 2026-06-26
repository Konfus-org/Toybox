#pragma once
#include "frame_buffers.h"
#include "gpu_resource_cache.h"
#include "gpu_resources.h"
#include "post_processor.h"
#include "world_view.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/assets/shader.h"
#include "tbx/types/color.h"
#include "tbx/types/handle.h"
#include "tbx/types/size.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <algorithm>
#include <array>
#include <memory>
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: The GPU state the pipeline's built-in passes SHARE: the resource cache, the world-view
    /// extractor, the reusable transient-buffer pool, and the per-frame state prepare_frame fills.
    /// Resources owned by a single pass (the shadow maps, the post targets) live on that pass, not here.
    /// Owned by RenderingPipeline and handed to its pass objects by reference. Thread Safety:
    /// Render-lane only.
    struct PipelineResources
    {
        PipelineResources(std::weak_ptr<IGraphicsBackend> backend, std::weak_ptr<AssetManager> assets)
            : cache(backend, std::move(assets))
            , frame_buffers(backend)
        {
        }

        GpuResourceCache cache;
        WorldView view = {};
        // Reusable transient-buffer pool, rewound each frame (begin()) and reused across frames so the
        // steady state allocates no new per-frame GPU buffers.
        FrameBuffers frame_buffers;

        // Transient per-frame GPU state the built-in passes share, set by prepare_frame each frame: the
        // captured world view, the frame's world bind group (owned here so it outlives prepare_frame),
        // and the shadow→forward sample-group blackboard (the shadow pass writes it, the forward pass
        // reads it). Valid only for the duration of one execute().
        const WorldViewResult* frame_view = nullptr;
        GpuResource frame_world_group = {};
        GpuResource frame_shadow_sample_group = {};
    };

    //// SHARED PASS HELPERS ////

    inline const Color SKY_COLOR = Color(0.45F, 0.62F, 0.86F, 1.0F);

    // Shadow caster shaders: ShadowDepth writes the opaque depth map; ShadowColor multiplies a
    // transparent caster's tint into the transmittance map. Referenced by path like every built-in.
    inline const Handle SHADOW_DEPTH_VERTEX_SHADER_HANDLE = Handle("Shaders/Material/ShadowDepth.vert");
    inline const Handle SHADOW_DEPTH_FRAGMENT_SHADER_HANDLE = Handle("Shaders/Material/ShadowDepth.frag");
    inline const Handle SHADOW_COLOR_VERTEX_SHADER_HANDLE = Handle("Shaders/Material/ShadowColor.vert");
    inline const Handle SHADOW_COLOR_FRAGMENT_SHADER_HANDLE = Handle("Shaders/Material/ShadowColor.frag");

    // Tag mask: tagged entities are drawn flat (white) into the tag mask target. Reuses the SSBO-pull
    // Fallback vertex stage (camera viewProjection) with a flat white fragment; a tag-gated post effect
    // (e.g. an outline) samples the result.
    inline const Handle MASK_VERTEX_SHADER_HANDLE = Handle("Shaders/Material/Fallback.vert");
    inline const Handle MASK_FRAGMENT_SHADER_HANDLE = Handle("Shaders/Post/SelectionMask.frag");

    inline ShaderProgram shader_program(const Handle& vertex, const Handle& fragment)
    {
        auto program = ShaderProgram {};
        program.vertex = vertex;
        program.fragment = fragment;
        return program;
    }

    // Opaque caster state: writes depth, no blending. `two_sided` disables face culling so the
    // material's sidedness is honored (single-sided casters cull their back faces). Shadow acne is
    // handled by the slope-scaled bias in the sampling shader, not polygon offset.
    inline RasterState shadow_depth_state(bool two_sided)
    {
        return RasterState {
            .is_blending_enabled = false,
            .is_two_sided = two_sided,
            .is_depth_test_enabled = true,
            .is_depth_write_enabled = true,
            .depth_function = MaterialDepthFunction::LESS,
            .blend_equation = BlendEquation::ALPHA};
    }

    // Transparent caster state: multiplicatively accumulates transmittance into the white-cleared
    // color map. Depth-tests against (but does not write) the opaque depth map so transparent casters
    // occluded by opaque geometry don't tint, while stacked transparent layers multiply together.
    inline RasterState shadow_color_state(bool two_sided)
    {
        return RasterState {
            .is_blending_enabled = true,
            .is_two_sided = two_sided,
            .is_depth_test_enabled = true,
            .is_depth_write_enabled = false,
            .depth_function = MaterialDepthFunction::LESS,
            .blend_equation = BlendEquation::MULTIPLY};
    }

    inline ResourceBinding storage_binding(uint32 slot, GpuId buffer)
    {
        return ResourceBinding {.binding_slot = slot, .resource_handle = buffer};
    }

    // Per-cascade shadow-map resolution: the configured base resolution (cascade 0) halved each
    // cascade, clamped to a floor so the furthest cascade stays usable. With a 2048 base this yields
    // 2048 / 1024 / 512 / 256 — the furthest cascade is deliberately low resolution since it spreads
    // over the whole far range.
    inline constexpr uint32 SHADOW_MIN_CASCADE_RESOLUTION = 256U;

    inline uint32 cascade_resolution(uint32 base_resolution, uint32 cascade)
    {
        const uint32 scaled = std::max(base_resolution, SHADOW_MIN_CASCADE_RESOLUTION) >> cascade;
        return std::max(scaled, SHADOW_MIN_CASCADE_RESOLUTION);
    }

    // Per-layer resolution of the local light shadow atlas, derived from the configured base resolution
    // but capped: the atlas holds MAX_LOCAL_SHADOW_VIEWS layers, so an uncapped 4K base would cost
    // hundreds of MB. Clamped to a sharp-but-affordable band.
    inline constexpr uint32 LOCAL_SHADOW_MIN_RESOLUTION = 512U;
    inline constexpr uint32 LOCAL_SHADOW_MAX_RESOLUTION = 1024U;

    inline uint32 local_shadow_resolution(uint32 base_resolution)
    {
        return std::clamp(base_resolution / 2U, LOCAL_SHADOW_MIN_RESOLUTION, LOCAL_SHADOW_MAX_RESOLUTION);
    }

    inline Result clear_swapchain(IGraphicsBackend& backend, const Color& color, const Size& size)
    {
        auto pass = RenderPassDesc {.clear_color = color, .clear_flags = ClearFlags::COLOR_DEPTH};
        pass.viewport.dimensions = size;
        if (auto result = backend.begin_render_pass(pass); !result)
            return result;
        return backend.end_render_pass();
    }

    // Builds the world bind group spanning this frame's transient buffers (instances, lights, uniforms)
    // and the persistent cache (vertices, materials, bindless textures) plus the global index element
    // buffer. Lives only for the frame.
    inline Result build_world_bind_group(
        IGraphicsBackend& backend,
        GpuResourceCache& cache,
        GpuId instances_buffer,
        GpuId lights_buffer,
        GpuId uniforms_buffer,
        GpuId local_shadow_matrices_buffer,
        std::weak_ptr<IGraphicsBackend> backend_weak,
        GpuResource& out_group)
    {
        const auto buffer = [&cache](CacheId id)
        {
            return cache.get_buffer(id).value_or(INVALID_GPU_ID);
        };
        auto desc = BindGroupDesc {
            .bindings = {
                storage_binding(GPU_BINDING_ALL_INSTANCES, instances_buffer),
                storage_binding(GPU_BINDING_GLOBAL_VERTICES, buffer(VERTICES_BUFFER_ID)),
                storage_binding(GPU_BINDING_LOCAL_SHADOW_MATRICES, local_shadow_matrices_buffer),
                storage_binding(GPU_BINDING_GLOBAL_MATERIALS, buffer(MATERIAL_TABLE_BUFFER_ID)),
                storage_binding(GPU_BINDING_GLOBAL_LIGHTS, lights_buffer),
                storage_binding(GPU_BINDING_GLOBAL_TEXTURES, buffer(TEXTURE_TABLE_BUFFER_ID)),
                storage_binding(GPU_BINDING_UNIFORMS, uniforms_buffer), // inferred UBO by usage
                storage_binding(0U, buffer(INDICES_BUFFER_ID))}}; // element buffer (INDEX usage)
        auto id = INVALID_GPU_ID;
        if (auto result = backend.create_bind_group(desc, id); !result)
            return result;
        out_group = GpuResource(std::move(backend_weak), id);
        return Result::OK;
    }
}
