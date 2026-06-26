#include "shadow_pass.h"
#include "../pipeline_internal.h"
#include "tbx/systems/debugging/macros.h"
#include <array>
#include <utility>

namespace tbx
{
    ShadowPass::ShadowPass(PipelineResources& resources, std::weak_ptr<IGraphicsBackend> backend)
        : RenderPass(PassType::Scene)
        , _resources(resources)
        , _backend(std::move(backend))
    {
    }

    Result ShadowPass::execute(FramePassContext& context)
    {
        // Bind the frame inputs (shared, on _resources) and this pass's own shadow targets to the names
        // the stage body uses, so the pass logic reads as it did when it was a standalone routine.
        IGraphicsBackend& backend = context.backend;
        const WorldViewResult& view = *_resources.frame_view;
        FrameBuffers& frame = _resources.frame_buffers;
        const GraphicsSettings& settings = context.settings;
        const GpuResource& world_group = _resources.frame_world_group;
        const uint32 stride = context.stride;
        const World* world = context.world;
        const uint64 frame_epoch = context.frame_epoch;
        GpuResource& out_sample_group = _resources.frame_shadow_sample_group;
        auto& backend_weak = _backend;
        auto& cache = _resources.cache;
        auto& shadow_cascades = _shadow_cascades;
        auto& shadow_cascade_sizes = _shadow_cascade_sizes;
        auto& shadow_color_maps = _shadow_color_maps;
        auto& shadow_base_resolution = _shadow_base_resolution;
        auto& local_shadow_atlas = _local_shadow_atlas;
        auto& local_shadow_resolution = _local_shadow_resolution;
        auto& last_local_atlas_epoch = _last_local_atlas_epoch;
        auto& last_local_atlas_world = _last_local_atlas_world;

        const bool has_directional_shadows = view.uniforms.shadow_count > 0U;
        const bool has_local_shadows = !view.local_shadow_matrices.empty();
        if (!has_directional_shadows && !has_local_shadows)
            return Result::OK;

        // The local atlas is camera-independent, so it only needs regenerating when the frame or the
        // rendered world changes (or the atlas was (re)created) — not for every view of the same
        // frame. Directional cascades are camera-fit and always re-render below.
        const bool render_local_atlas = has_local_shadows
            && (frame_epoch != last_local_atlas_epoch || world != last_local_atlas_world
                || !local_shadow_atlas.is_valid());

        // shadow_map_resolution is a Clamp<uint32, 256> so it can never drop below the floor the
        // cascade atlas needs — read it straight through.
        const uint32 base_resolution = settings.shadow_map_resolution;
        if (has_directional_shadows
            && (base_resolution != shadow_base_resolution || !shadow_cascades[0].is_valid()))
        {
            for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
            {
                const uint32 res = cascade_resolution(base_resolution, c);
                auto depth_desc = TextureDesc {
                    .usage = TextureUsage::SAMPLED_DEPTH_STENCIL,
                    .format = TextureFormat::DEPTH32_FLOAT,
                    .size = Size {res, res},
                    .is_depth_comparison_enabled = false,
                    .is_linear_filtering_enabled = false};
                auto depth_id = INVALID_GPU_ID;
                if (auto result = backend.create_texture(depth_desc, depth_id); !result)
                    return result;
                shadow_cascades[c] = GpuResource(backend_weak, depth_id);
                shadow_cascade_sizes[c] = res;

                // Matching colored transmittance map per cascade: same resolution + projection as the
                // depth map so it registers exactly, and so colored glass shadows stay sharp near and
                // reach far together with the depth cascades.
                auto color_desc = TextureDesc {
                    .usage = TextureUsage::SAMPLED_RENDER_TARGET,
                    .format = TextureFormat::RGBA8,
                    .size = Size {res, res},
                    .is_linear_filtering_enabled = false};
                auto color_id = INVALID_GPU_ID;
                if (auto result = backend.create_texture(color_desc, color_id); !result)
                    return result;
                shadow_color_maps[c] = GpuResource(backend_weak, color_id);
            }

            shadow_base_resolution = base_resolution;
        }

        // Local shadow atlas: one DEPTH32 texture array of MAX_LOCAL_SHADOW_VIEWS layers, recreated
        // only when the derived per-layer resolution changes.
        const uint32 local_resolution = tbx::local_shadow_resolution(base_resolution);
        if (has_local_shadows
            && (local_resolution != local_shadow_resolution || !local_shadow_atlas.is_valid()))
        {
            auto atlas_desc = TextureDesc {
                .usage = TextureUsage::SAMPLED_DEPTH_STENCIL,
                .format = TextureFormat::DEPTH32_FLOAT,
                .size = Size {local_resolution, local_resolution},
                .array_layer_count = MAX_LOCAL_SHADOW_VIEWS,
                .is_depth_comparison_enabled = false,
                .is_linear_filtering_enabled = false};
            auto atlas_id = INVALID_GPU_ID;
            if (auto result = backend.create_texture(atlas_desc, atlas_id); !result)
                return result;
            local_shadow_atlas = GpuResource(backend_weak, atlas_id);
            local_shadow_resolution = local_resolution;
        }

        // Upload the caster commands (concatenated in ShadowCasterCategory order). Empty means a
        // caster light exists but no geometry casts this frame — the passes still clear the maps.
        GpuId shadow_args_buffer = INVALID_GPU_ID;
        if (!view.shadow_draw_commands.empty())
        {
            shadow_args_buffer = frame.store(
                view.shadow_draw_commands.data(),
                view.shadow_draw_commands.size() * sizeof(GpuIndexedDrawCommand),
                BufferUsage::INDIRECT_ARGS);
            if (shadow_args_buffer == INVALID_GPU_ID)
                return Result(false, "Failed to upload shadow caster commands.");
        }

        // Category command offsets into the shadow args buffer (prefix sum).
        std::array<uint32, SHADOW_CASTER_CATEGORY_COUNT> offsets = {};
        for (uint32 category = 1U; category < SHADOW_CASTER_CATEGORY_COUNT; ++category)
            offsets[category] = offsets[category - 1U] + view.shadow_category_counts[category - 1U];

        const auto add_shadow_pipeline =
            [&](const ShaderProgram& program, const RasterState& state) -> GpuId
        {
            return cache.add_pipeline(hash(program, state), program, state, true)
                .value_or(INVALID_GPU_ID);
        };
        const ShaderProgram depth_program = shader_program(
            SHADOW_DEPTH_VERTEX_SHADER_HANDLE,
            SHADOW_DEPTH_FRAGMENT_SHADER_HANDLE);
        const ShaderProgram color_program = shader_program(
            SHADOW_COLOR_VERTEX_SHADER_HANDLE,
            SHADOW_COLOR_FRAGMENT_SHADER_HANDLE);
        // One pipeline per caster category (indexed by ShadowCasterCategory).
        const std::array<GpuId, SHADOW_CASTER_CATEGORY_COUNT> shadow_pipelines = {
            add_shadow_pipeline(depth_program, shadow_depth_state(false)),
            add_shadow_pipeline(depth_program, shadow_depth_state(true)),
            add_shadow_pipeline(color_program, shadow_color_state(false)),
            add_shadow_pipeline(color_program, shadow_color_state(true))};

        // Uploads one caster view matrix as a per-view UBO and wraps it in a bind group the caster
        // vertex shaders read at GPU_BINDING_SHADOW_PASS_UNIFORMS. Used for cascade and local views.
        const auto make_matrix_group =
            [&](const Mat4& view_projection, GpuResource& out_group) -> Result
        {
            const auto matrix = GpuShadowPassUniforms {.view_projection = view_projection};
            const GpuId matrix_buffer =
                frame.store(&matrix, sizeof(GpuShadowPassUniforms), BufferUsage::UNIFORM);
            if (matrix_buffer == INVALID_GPU_ID)
                return Result(false, "Failed to upload shadow view matrix.");
            auto group_desc = BindGroupDesc {
                .bindings = {ResourceBinding {
                    .binding_slot = GPU_BINDING_SHADOW_PASS_UNIFORMS,
                    .resource_handle = matrix_buffer}}};
            auto group_id = INVALID_GPU_ID;
            if (auto result = backend.create_bind_group(group_desc, group_id); !result)
                return result;
            out_group = GpuResource(backend_weak, group_id);
            return Result::OK;
        };

        // Binds the category's pipeline + world group + the given view's matrix group and issues its
        // indirect draw. Skips empty categories and ones whose pipeline failed to compile (the map
        // simply stays cleared).
        const auto draw_category = [&](uint32 category, GpuId matrix_group) -> Result
        {
            const uint32 count = view.shadow_category_counts[category];
            const GpuId pipeline = shadow_pipelines[category];
            if (count == 0U || pipeline == INVALID_GPU_ID)
                return Result::OK;
            if (auto result = backend.bind_raster_pipeline(pipeline); !result)
                return result;
            if (auto result = backend.bind_group(0U, world_group.get()); !result)
                return result;
            if (auto result = backend.bind_group(1U, matrix_group); !result)
                return result;
            return backend.draw_indirect(
                shadow_args_buffer,
                static_cast<uint64>(offsets[category]) * stride,
                count,
                stride);
        };

        if (has_directional_shadows)
        {
            // Per-cascade caster matrix groups (one per cascade box).
            std::array<GpuResource, SHADOW_CASCADE_COUNT> cascade_matrix_groups = {};
            for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
                if (auto result = make_matrix_group(
                        view.uniforms.cascade_view_projection[c],
                        cascade_matrix_groups[c]);
                    !result)
                    return result;

            // (1) Opaque depth sub-pass, one per cascade.
            for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
            {
                const Size cascade_size {shadow_cascade_sizes[c], shadow_cascade_sizes[c]};
                auto depth_pass = RenderPassDesc {
                    .depth_stencil_target = shadow_cascades[c].get(),
                    .clear_depth = 1.0F,
                    .clear_flags = ClearFlags::DEPTH,
                    .is_color_write_enabled = false};
                depth_pass.viewport.dimensions = cascade_size;
                if (auto result = backend.begin_render_pass(depth_pass); !result)
                    return result;
                if (auto result = draw_category(
                        SHADOW_CASTER_OPAQUE_ONE_SIDED,
                        cascade_matrix_groups[c].get());
                    !result)
                    return result;
                if (auto result = draw_category(
                        SHADOW_CASTER_OPAQUE_TWO_SIDED,
                        cascade_matrix_groups[c].get());
                    !result)
                    return result;
                if (auto result = backend.end_render_pass(); !result)
                    return result;
            }

            // (2) Translucent transmittance sub-pass, one per cascade (its own projection). Each clears
            // its color map to white (fully transmissive) but keeps that cascade's opaque depth so
            // transparent casters behind opaque geometry are depth-rejected. Mirrors the per-cascade
            // depth passes so colored glass shadows are sharp near and reach far.
            for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
            {
                const GpuId color_matrix_group = cascade_matrix_groups[c].get();
                const Size color_size {shadow_cascade_sizes[c], shadow_cascade_sizes[c]};
                auto color_pass = RenderPassDesc {
                    .color_targets = {shadow_color_maps[c].get()},
                    .depth_stencil_target = shadow_cascades[c].get(),
                    .clear_color = Color(1.0F, 1.0F, 1.0F, 1.0F),
                    .clear_flags = ClearFlags::COLOR};
                color_pass.viewport.dimensions = color_size;
                if (auto result = backend.begin_render_pass(color_pass); !result)
                    return result;
                if (auto result =
                        draw_category(SHADOW_CASTER_TRANSPARENT_ONE_SIDED, color_matrix_group);
                    !result)
                    return result;
                if (auto result =
                        draw_category(SHADOW_CASTER_TRANSPARENT_TWO_SIDED, color_matrix_group);
                    !result)
                    return result;
                if (auto result = backend.end_render_pass(); !result)
                    return result;
            }
        }

        if (render_local_atlas)
        {
            // One opaque depth sub-pass per local shadow view, each rendering the casters into its own
            // atlas layer through that view's matrix. Only opaque casters block local lights
            // (transparent casters are skipped — glass barely occludes a lamp), so no colored
            // transmittance pass is needed here. Rendered once per frame/world; reused across views.
            const Size atlas_size {local_shadow_resolution, local_shadow_resolution};
            for (uint32 layer = 0U; layer < view.local_shadow_matrices.size(); ++layer)
            {
                GpuResource view_matrix_group = {};
                if (auto result =
                        make_matrix_group(view.local_shadow_matrices[layer], view_matrix_group);
                    !result)
                    return result;

                auto depth_pass = RenderPassDesc {
                    .depth_stencil_target = local_shadow_atlas.get(),
                    .depth_stencil_layer = static_cast<int32>(layer),
                    .clear_depth = 1.0F,
                    .clear_flags = ClearFlags::DEPTH,
                    .is_color_write_enabled = false};
                depth_pass.viewport.dimensions = atlas_size;
                if (auto result = backend.begin_render_pass(depth_pass); !result)
                    return result;
                if (auto result =
                        draw_category(SHADOW_CASTER_OPAQUE_ONE_SIDED, view_matrix_group.get());
                    !result)
                    return result;
                if (auto result =
                        draw_category(SHADOW_CASTER_OPAQUE_TWO_SIDED, view_matrix_group.get());
                    !result)
                    return result;
                if (auto result = backend.end_render_pass(); !result)
                    return result;
            }

            // Remember which frame + world the atlas now holds so the other views of this frame (and
            // repeat frames of an unchanged scene's first view onward) reuse it instead of redrawing.
            last_local_atlas_epoch = frame_epoch;
            last_local_atlas_world = world;
        }

        // Sample group for the forward pass: the cascade depth + colored transmittance maps (when a
        // directional caster is active) and the local light shadow atlas (when any local light casts).
        // Each binding lands on its consecutive slot; absent families simply aren't bound.
        auto sample_desc = BindGroupDesc {};
        if (has_directional_shadows)
        {
            for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
                sample_desc.bindings.push_back(
                    ResourceBinding {
                        .binding_slot = GPU_BINDING_SHADOW_CASCADE_BASE + c,
                        .resource_handle = shadow_cascades[c].get()});
            for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
                sample_desc.bindings.push_back(
                    ResourceBinding {
                        .binding_slot = GPU_BINDING_SHADOW_COLOR_BASE + c,
                        .resource_handle = shadow_color_maps[c].get()});
        }
        if (has_local_shadows)
            sample_desc.bindings.push_back(
                ResourceBinding {
                    .binding_slot = GPU_BINDING_LOCAL_SHADOW_ATLAS,
                    .resource_handle = local_shadow_atlas.get()});
        auto sample_group_id = INVALID_GPU_ID;
        if (auto result = backend.create_bind_group(sample_desc, sample_group_id); !result)
            return result;
        out_sample_group = GpuResource(backend_weak, sample_group_id);
        return Result::OK;
    }
}
