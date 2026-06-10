#include "tbx/systems/graphics/rendering_pipeline.h"
#include "rendering_pipeline/frame_buffers.h"
#include "rendering_pipeline/gpu_resource_cache.h"
#include "rendering_pipeline/post_processor.h"
#include "rendering_pipeline/world_view.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/assets/shader.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/color.h"
#include "tbx/types/handle.h"
#include "tbx/types/size.h"
#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include <vector>

namespace tbx
{
    struct RenderingPipeline::Resources
    {
        Resources(std::weak_ptr<IGraphicsBackend> backend, std::weak_ptr<AssetManager> assets)
            : cache(backend, std::move(assets))
            , post(backend)
        {
        }

        GpuResourceCache cache;
        WorldView view = {};
        PostProcessor post;

        // Persistent directional shadow cascades reused across frames; recreated only when the
        // configured base resolution changes. Each cascade is an opaque depth map at a decreasing
        // resolution (cascade 0 sharpest, furthest lowest). shadow_color_map is the translucent
        // transmittance map transparent casters multiply into (white = fully transmissive), shared
        // with the furthest cascade's projection so it covers the whole shadowed range.
        std::array<GpuResource, SHADOW_CASCADE_COUNT> shadow_cascades = {};
        std::array<uint32, SHADOW_CASCADE_COUNT> shadow_cascade_sizes = {};
        GpuResource shadow_color_map = {};
        uint32 shadow_base_resolution = 0U;
    };

    //// STATIC HELPERS ////

    static const Color SKY_COLOR = Color(0.45F, 0.62F, 0.86F, 1.0F);

    // Shadow caster shaders: ShadowDepth writes the opaque depth map; ShadowColor multiplies a
    // transparent caster's tint into the transmittance map. Referenced by path like every built-in.
    static const Handle SHADOW_DEPTH_VERTEX_SHADER_HANDLE =
        Handle("Shaders/Material/ShadowDepth.vert");
    static const Handle SHADOW_DEPTH_FRAGMENT_SHADER_HANDLE =
        Handle("Shaders/Material/ShadowDepth.frag");
    static const Handle SHADOW_COLOR_VERTEX_SHADER_HANDLE =
        Handle("Shaders/Material/ShadowColor.vert");
    static const Handle SHADOW_COLOR_FRAGMENT_SHADER_HANDLE =
        Handle("Shaders/Material/ShadowColor.frag");

    static ShaderProgram shader_program(const Handle& vertex, const Handle& fragment)
    {
        auto program = ShaderProgram {};
        program.vertex = vertex;
        program.fragment = fragment;
        return program;
    }

    // Opaque caster state: writes depth, no blending. `two_sided` disables face culling so the
    // material's sidedness is honored (single-sided casters cull their back faces). Shadow acne is
    // handled by the slope-scaled bias in the sampling shader, not polygon offset.
    static RasterState shadow_depth_state(bool two_sided)
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
    static RasterState shadow_color_state(bool two_sided)
    {
        return RasterState {
            .is_blending_enabled = true,
            .is_two_sided = two_sided,
            .is_depth_test_enabled = true,
            .is_depth_write_enabled = false,
            .depth_function = MaterialDepthFunction::LESS,
            .blend_equation = BlendEquation::MULTIPLY};
    }

    static ResourceBinding storage_binding(uint32 slot, GpuId buffer)
    {
        return ResourceBinding {.binding_slot = slot, .resource_handle = buffer};
    }

    // Per-cascade shadow-map resolution: the configured base resolution (cascade 0) halved each
    // cascade, clamped to a floor so the furthest cascade stays usable. With a 2048 base this yields
    // 2048 / 1024 / 512 / 256 — the furthest cascade is deliberately low resolution since it spreads
    // over the whole far range.
    static constexpr uint32 SHADOW_MIN_CASCADE_RESOLUTION = 256U;

    static uint32 cascade_resolution(uint32 base_resolution, uint32 cascade)
    {
        const uint32 scaled = std::max(base_resolution, SHADOW_MIN_CASCADE_RESOLUTION) >> cascade;
        return std::max(scaled, SHADOW_MIN_CASCADE_RESOLUTION);
    }

    static Result clear_swapchain(IGraphicsBackend& backend, const Color& color, const Size& size)
    {
        auto pass = RenderPassDesc {.clear_color = color, .clear_flags = ClearFlags::COLOR_DEPTH};
        pass.viewport.dimensions = size;
        if (auto result = backend.begin_render_pass(pass); !result)
            return result;
        return backend.end_render_pass();
    }

    // Builds the world bind group spanning this frame's transient buffers (instances, lights,
    // uniforms) and the persistent cache (vertices, materials, bindless textures) plus the global
    // index element buffer. Lives only for the frame.
    static Result build_world_bind_group(
        IGraphicsBackend& backend,
        GpuResourceCache& cache,
        GpuId instances_buffer,
        GpuId lights_buffer,
        GpuId uniforms_buffer,
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

    //// RenderingPipeline ////

    RenderingPipeline::RenderingPipeline(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<IWindowManager> window_manager,
        std::weak_ptr<WorldManager> world_manager)
        : _backend(std::move(backend))
        , _asset_manager(std::move(asset_manager))
        , _window_manager(std::move(window_manager))
        , _world_manager(std::move(world_manager))
        , _resources(std::make_unique<Resources>(_backend, _asset_manager))
    {
    }

    RenderingPipeline::RenderingPipeline(
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<IWindowManager> window_manager,
        std::weak_ptr<WorldManager> world_manager)
        : _asset_manager(std::move(asset_manager))
        , _window_manager(std::move(window_manager))
        , _world_manager(std::move(world_manager))
        , _resources(std::make_unique<Resources>(_backend, _asset_manager))
    {
    }

    RenderingPipeline::RenderingPipeline(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<IWindowManager> window_manager)
        : RenderingPipeline(
              std::move(backend),
              std::move(asset_manager),
              std::move(window_manager),
              {})
    {
    }

    // RAII: the cache + world view free their backend resources on destruction. Defined here where
    // GpuResourceCache and WorldView are complete types.
    RenderingPipeline::~RenderingPipeline() = default;

    Result RenderingPipeline::execute(const GraphicsSettings& settings, const DeltaTime& delta_time)
    {
        const auto backend_service = _backend.lock();
        if (!backend_service)
            return Result(false, "Rendering pipeline has no graphics backend.");
        IGraphicsBackend& backend = *backend_service;

        _elapsed_time += static_cast<float>(delta_time.seconds);
        ++_frame_index;

        // Respect the active graphics settings: local lights/meshes beyond this camera distance are
        // dropped (a value <= 0 means unbounded).
        const float light_cull_distance = settings.local_light_max_distance > 0.0F
                                              ? settings.local_light_max_distance
                                              : std::numeric_limits<float>::max();

        const auto window_manager = _window_manager.lock();
        if (!window_manager || !window_manager->has_main_window())
            return Result::OK;

        const Window output = window_manager->get_main_window();
        if (auto result = backend.begin_frame(output); !result)
            return result;

        const Size output_size = window_manager->get_size(output);
        const auto finish_frame = [&backend]() -> Result
        {
            const auto present_result = backend.present();
            const auto end_result = backend.end_frame();
            return present_result ? end_result : present_result;
        };
        const auto fail_frame =
            [&backend, &finish_frame, &output_size](const Result& reason) -> Result
        {
            TBX_TRACE_ERROR_ONCE("Forward+ pipeline frame failed: {}", reason.get_report());
            clear_swapchain(backend, Color::MAGENTA, output_size);
            finish_frame();
            return reason;
        };

        _resources->cache.update(delta_time);

        const auto asset_manager = _asset_manager.lock();
        const auto world_manager = _world_manager.lock();
        if (!asset_manager || !world_manager || !world_manager->has_active_world())
        {
            clear_swapchain(backend, Color::BLACK, output_size);
            return finish_frame();
        }
        const auto world = world_manager->get_active_world().lock();
        if (!world)
            return finish_frame();

        const WorldViewResult view = _resources->view.capture(
            *asset_manager,
            *world,
            _resources->cache,
            output_size,
            _elapsed_time,
            light_cull_distance,
            settings.shadow_render_distance,
            settings.shadow_softness);

        if (!view.has_camera || view.instances.empty())
        {
            clear_swapchain(backend, SKY_COLOR, output_size);
            return finish_frame();
        }

        // Upload this frame's transient buffers (freed when `frame` goes out of scope).
        FrameBuffers frame(_backend);
        const GpuId instances_buffer = frame.store(
            view.instances.data(),
            view.instances.size() * sizeof(GpuInstanceData),
            BufferUsage::STORAGE);
        const GpuId lights_buffer = frame.store(
            view.lights.data(),
            view.lights.size() * sizeof(GpuLightData),
            BufferUsage::STORAGE);
        const GpuId uniforms_buffer =
            frame.store(&view.uniforms, sizeof(GpuUniforms), BufferUsage::UNIFORM);
        const GpuId draw_args_buffer = frame.store(
            view.draw_commands.data(),
            view.draw_commands.size() * sizeof(GpuIndexedDrawCommand),
            BufferUsage::INDIRECT_ARGS);
        if (instances_buffer == INVALID_GPU_ID || lights_buffer == INVALID_GPU_ID
            || uniforms_buffer == INVALID_GPU_ID || draw_args_buffer == INVALID_GPU_ID)
            return fail_frame(Result(false, "Failed to upload per-frame buffers."));

        GpuResource world_group = {};
        if (auto result = build_world_bind_group(
                backend,
                _resources->cache,
                instances_buffer,
                lights_buffer,
                uniforms_buffer,
                _backend,
                world_group);
            !result)
            return fail_frame(result);

        // When post-processing is active the forward pass renders into an offscreen scene target
        // the effect chain consumes; otherwise it renders straight to the swapchain (unchanged
        // path).
        const bool use_post = _resources->post.wants_post(*world);
        if (use_post)
        {
            if (auto result = _resources->post.ensure_targets(output_size); !result)
                return fail_frame(result);
        }

        constexpr uint32 stride = static_cast<uint32>(sizeof(GpuIndexedDrawCommand));

        //// SHADOW PASS ////
        // Cascaded directional shadows. One opaque depth sub-pass per cascade (each its own
        // camera-centered ortho box + decreasing resolution; cascade 0 sharp/near, the furthest low-
        // res/far) writes the depth map the forward pass occlusion-tests against. A final translucent
        // sub-pass multiplies transparent casters' tint into a white-cleared color map (rendered in
        // the furthest cascade's full-range projection, depth-tested against that cascade's depth,
        // depth-write off) so glass casts a colored, partial shadow across the whole range. The sky
        // and ShadowMode::OFF materials were already excluded by world_view. Each caster sub-pass
        // reads its cascade's matrix from a small per-cascade UBO. The forward pass samples all
        // cascades (slots 12..) + the color map; shadow_sample_group is null when there is no caster.
        GpuResource shadow_sample_group = {};
        if (view.uniforms.shadow_count > 0U)
        {
            const uint32 base_resolution = std::max(settings.shadow_map_resolution, 256U);
            if (base_resolution != _resources->shadow_base_resolution
                || !_resources->shadow_cascades[0].is_valid())
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
                        return fail_frame(result);
                    _resources->shadow_cascades[c] = GpuResource(_backend, depth_id);
                    _resources->shadow_cascade_sizes[c] = res;
                }

                // Colored transmittance shares the furthest cascade's projection + resolution so it
                // can depth-test against that cascade's depth map.
                const uint32 color_res =
                    cascade_resolution(base_resolution, SHADOW_CASCADE_COUNT - 1U);
                auto color_desc = TextureDesc {
                    .usage = TextureUsage::SAMPLED_RENDER_TARGET,
                    .format = TextureFormat::RGBA8,
                    .size = Size {color_res, color_res},
                    .is_linear_filtering_enabled = false};
                auto color_id = INVALID_GPU_ID;
                if (auto result = backend.create_texture(color_desc, color_id); !result)
                    return fail_frame(result);
                _resources->shadow_color_map = GpuResource(_backend, color_id);

                _resources->shadow_base_resolution = base_resolution;
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
                    return fail_frame(Result(false, "Failed to upload shadow caster commands."));
            }

            // Category command offsets into the shadow args buffer (prefix sum).
            std::array<uint32, SHADOW_CASTER_CATEGORY_COUNT> offsets = {};
            for (uint32 category = 1U; category < SHADOW_CASTER_CATEGORY_COUNT; ++category)
                offsets[category] =
                    offsets[category - 1U] + view.shadow_category_counts[category - 1U];

            const auto add_shadow_pipeline = [&](const ShaderProgram& program,
                                                 const RasterState& state) -> GpuId
            {
                return _resources->cache
                    .add_pipeline(hash(program, state), program, state, true)
                    .value_or(INVALID_GPU_ID);
            };
            const ShaderProgram depth_program = shader_program(
                SHADOW_DEPTH_VERTEX_SHADER_HANDLE, SHADOW_DEPTH_FRAGMENT_SHADER_HANDLE);
            const ShaderProgram color_program = shader_program(
                SHADOW_COLOR_VERTEX_SHADER_HANDLE, SHADOW_COLOR_FRAGMENT_SHADER_HANDLE);
            // One pipeline per caster category (indexed by ShadowCasterCategory).
            const std::array<GpuId, SHADOW_CASTER_CATEGORY_COUNT> shadow_pipelines = {
                add_shadow_pipeline(depth_program, shadow_depth_state(false)),
                add_shadow_pipeline(depth_program, shadow_depth_state(true)),
                add_shadow_pipeline(color_program, shadow_color_state(false)),
                add_shadow_pipeline(color_program, shadow_color_state(true))};

            // Per-cascade caster matrix UBOs + bind groups (the caster vertex shaders transform by the
            // active cascade's matrix at binding GPU_BINDING_SHADOW_PASS_UNIFORMS). The furthest
            // cascade's group is reused for the colored transmittance pass (same full-range matrix).
            std::array<GpuResource, SHADOW_CASCADE_COUNT> cascade_matrix_groups = {};
            for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
            {
                const auto matrix = GpuShadowPassUniforms {
                    .view_projection = view.uniforms.cascade_view_projection[c]};
                const GpuId matrix_buffer =
                    frame.store(&matrix, sizeof(GpuShadowPassUniforms), BufferUsage::UNIFORM);
                if (matrix_buffer == INVALID_GPU_ID)
                    return fail_frame(Result(false, "Failed to upload cascade matrix."));
                auto group_desc = BindGroupDesc {
                    .bindings = {ResourceBinding {
                        .binding_slot = GPU_BINDING_SHADOW_PASS_UNIFORMS,
                        .resource_handle = matrix_buffer}}};
                auto group_id = INVALID_GPU_ID;
                if (auto result = backend.create_bind_group(group_desc, group_id); !result)
                    return fail_frame(result);
                cascade_matrix_groups[c] = GpuResource(_backend, group_id);
            }

            // Binds the category's pipeline + world group + the given cascade matrix group and issues
            // its indirect draw. Skips empty categories and ones whose pipeline failed to compile (the
            // map simply stays cleared).
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

            // (1) Opaque depth sub-pass, one per cascade.
            for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
            {
                const Size cascade_size {
                    _resources->shadow_cascade_sizes[c], _resources->shadow_cascade_sizes[c]};
                auto depth_pass = RenderPassDesc {
                    .depth_stencil_target = _resources->shadow_cascades[c].get(),
                    .clear_depth = 1.0F,
                    .clear_flags = ClearFlags::DEPTH,
                    .is_color_write_enabled = false};
                depth_pass.viewport.dimensions = cascade_size;
                if (auto result = backend.begin_render_pass(depth_pass); !result)
                    return fail_frame(result);
                if (auto result =
                        draw_category(SHADOW_CASTER_OPAQUE_ONE_SIDED, cascade_matrix_groups[c].get());
                    !result)
                    return fail_frame(result);
                if (auto result =
                        draw_category(SHADOW_CASTER_OPAQUE_TWO_SIDED, cascade_matrix_groups[c].get());
                    !result)
                    return fail_frame(result);
                if (auto result = backend.end_render_pass(); !result)
                    return fail_frame(result);
            }

            // (2) Translucent transmittance sub-pass (furthest cascade's full-range projection).
            // Clears color to white (fully transmissive) but keeps the furthest cascade's opaque depth
            // so transparent casters behind opaque geometry are depth-rejected.
            const uint32 last = SHADOW_CASCADE_COUNT - 1U;
            const GpuId color_matrix_group = cascade_matrix_groups[last].get();
            const Size color_size {
                _resources->shadow_cascade_sizes[last], _resources->shadow_cascade_sizes[last]};
            auto color_pass = RenderPassDesc {
                .color_targets = {_resources->shadow_color_map.get()},
                .depth_stencil_target = _resources->shadow_cascades[last].get(),
                .clear_color = Color(1.0F, 1.0F, 1.0F, 1.0F),
                .clear_flags = ClearFlags::COLOR};
            color_pass.viewport.dimensions = color_size;
            if (auto result = backend.begin_render_pass(color_pass); !result)
                return fail_frame(result);
            if (auto result =
                    draw_category(SHADOW_CASTER_TRANSPARENT_ONE_SIDED, color_matrix_group);
                !result)
                return fail_frame(result);
            if (auto result =
                    draw_category(SHADOW_CASTER_TRANSPARENT_TWO_SIDED, color_matrix_group);
                !result)
                return fail_frame(result);
            if (auto result = backend.end_render_pass(); !result)
                return fail_frame(result);

            // Sample group for the forward pass: all cascade depth maps (consecutive slots) + the
            // colored transmittance map.
            auto sample_desc = BindGroupDesc {};
            for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
                sample_desc.bindings.push_back(ResourceBinding {
                    .binding_slot = GPU_BINDING_SHADOW_CASCADE_BASE + c,
                    .resource_handle = _resources->shadow_cascades[c].get()});
            sample_desc.bindings.push_back(ResourceBinding {
                .binding_slot = GPU_BINDING_SHADOW_COLOR,
                .resource_handle = _resources->shadow_color_map.get()});
            auto sample_group_id = INVALID_GPU_ID;
            if (auto result = backend.create_bind_group(sample_desc, sample_group_id); !result)
                return fail_frame(result);
            shadow_sample_group = GpuResource(_backend, sample_group_id);
        }

        //// FORWARD PASS ////
        // each material shades itself against the light list.
        auto pass = RenderPassDesc {
            .clear_color = SKY_COLOR,
            .clear_depth = 1.0F,
            .clear_flags = ClearFlags::COLOR_DEPTH};
        pass.viewport.dimensions = output_size;
        if (use_post)
        {
            pass.color_targets = {_resources->post.get_scene_color()};
            pass.depth_stencil_target = _resources->post.get_scene_depth();
        }
        if (auto result = backend.begin_render_pass(pass); !result)
            return fail_frame(result);

        uint64 command_offset = 0U;
        const bool diag = _frame_index == 2U;
        if (diag)
            TBX_TRACE_WARNING(
                "DRAW DIAG: buckets={} instances={} lights={}",
                view.bucket_pipelines.size(),
                view.instances.size(),
                view.lights.size());
        for (uint32 bucket = 0U; bucket < view.bucket_pipelines.size(); ++bucket)
        {
            const uint32 count = view.bucket_command_counts[bucket];
            const GpuId pipeline = view.bucket_pipelines[bucket];
            if (diag)
                TBX_TRACE_WARNING(
                    "DRAW DIAG bucket {}: pipeline={} count={}",
                    bucket,
                    static_cast<uint64>(pipeline),
                    count);
            if (count != 0U && pipeline != INVALID_GPU_ID)
            {
                if (auto result = backend.bind_raster_pipeline(pipeline); !result)
                    return fail_frame(result);
                if (auto result = backend.bind_group(0U, world_group.get()); !result)
                    return fail_frame(result);
                if (shadow_sample_group.is_valid())
                    if (auto result = backend.bind_group(1U, shadow_sample_group.get()); !result)
                        return fail_frame(result);
                if (auto result = backend.draw_indirect(
                        draw_args_buffer,
                        command_offset * stride,
                        count,
                        stride);
                    !result)
                    return fail_frame(result);
            }
            command_offset += count;
        }

        if (auto result = backend.end_render_pass(); !result)
            return fail_frame(result);

        //// POST-PROCESSING ////
        // chain the enabled material-driven effects from the scene target to the
        // swapchain. The final effect presents; a broken effect is skipped (warned), never the
        // frame.
        if (use_post)
        {
            if (auto result = _resources->post.run(
                    _resources->cache,
                    *asset_manager,
                    *world,
                    output_size,
                    uniforms_buffer);
                !result)
                return fail_frame(result);
        }

        return finish_frame();
    }

    void RenderingPipeline::reload()
    {
        // Recreate the cache + world view so reloaded shader sources recompile and caches rebuild.
        _resources = std::make_unique<Resources>(_backend, _asset_manager);
    }
}
