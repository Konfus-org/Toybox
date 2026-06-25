#include "tbx/systems/graphics/rendering_pipeline.h"
#include "rendering_pipeline/frame_buffers.h"
#include "rendering_pipeline/gpu_resource_cache.h"
#include "rendering_pipeline/post_processor.h"
#include "rendering_pipeline/world_view.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/gizmos.h"
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
        // resolution (cascade 0 sharpest, furthest lowest). shadow_color_maps holds the matching
        // translucent transmittance map for each cascade (same projection + resolution as its depth
        // map) that transparent casters multiply into (white = fully transmissive), so glass casts
        // a colored, partial shadow at every distance.
        std::array<GpuResource, SHADOW_CASCADE_COUNT> shadow_cascades = {};
        std::array<uint32, SHADOW_CASCADE_COUNT> shadow_cascade_sizes = {};
        std::array<GpuResource, SHADOW_CASCADE_COUNT> shadow_color_maps = {};
        uint32 shadow_base_resolution = 0U;

        // Local (point/spot/area) light shadows share one depth texture-array atlas: every
        // shadow-casting local light occupies a contiguous run of layers (one for spot/area, six for
        // a point light's cube faces). Recreated only when the derived per-layer resolution changes.
        GpuResource local_shadow_atlas = {};
        uint32 local_shadow_resolution = 0U;
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

    // Tag mask: tagged entities are drawn flat (white) into the tag mask target. Reuses the SSBO-pull
    // Fallback vertex stage (camera viewProjection) with a flat white fragment; a tag-gated post effect
    // (e.g. an outline) samples the result.
    static const Handle MASK_VERTEX_SHADER_HANDLE =
        Handle("Shaders/Material/Fallback.vert");
    static const Handle MASK_FRAGMENT_SHADER_HANDLE =
        Handle("Shaders/Post/SelectionMask.frag");

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
    // color map. Depth-tests against (but does not write) the opaque depth map so transparent
    // casters occluded by opaque geometry don't tint, while stacked transparent layers multiply
    // together.
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
    // cascade, clamped to a floor so the furthest cascade stays usable. With a 2048 base this
    // yields 2048 / 1024 / 512 / 256 — the furthest cascade is deliberately low resolution since it
    // spreads over the whole far range.
    static constexpr uint32 SHADOW_MIN_CASCADE_RESOLUTION = 256U;

    static uint32 cascade_resolution(uint32 base_resolution, uint32 cascade)
    {
        const uint32 scaled = std::max(base_resolution, SHADOW_MIN_CASCADE_RESOLUTION) >> cascade;
        return std::max(scaled, SHADOW_MIN_CASCADE_RESOLUTION);
    }

    // Per-layer resolution of the local light shadow atlas, derived from the configured base
    // resolution but capped: the atlas holds MAX_LOCAL_SHADOW_VIEWS layers, so an uncapped 4K base
    // would cost hundreds of MB. Clamped to a sharp-but-affordable band.
    static constexpr uint32 LOCAL_SHADOW_MIN_RESOLUTION = 512U;
    static constexpr uint32 LOCAL_SHADOW_MAX_RESOLUTION = 1024U;

    static uint32 local_shadow_resolution(uint32 base_resolution)
    {
        return std::clamp(
            base_resolution / 2U,
            LOCAL_SHADOW_MIN_RESOLUTION,
            LOCAL_SHADOW_MAX_RESOLUTION);
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

    Result RenderingPipeline::execute(
        const GraphicsSettings& settings,
        const DeltaTime& delta_time,
        const CameraView& camera_view,
        const RenderTarget& output_target,
        Gizmos* gizmos,
        const std::vector<PostProcessingEffect>& extra_post_effects,
        World* world_override)
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

        if (auto result = backend.begin_frame(output_target); !result)
            return result;

        // The target arrives fully resolved from the main thread, size included.
        const auto output_size = output_target.size;
        const auto finish_frame =
            [this, &backend, &output_target, &output_size, &camera_view, gizmos]() -> Result
        {
            // Draw the frame's gizmos on top of the finished scene before presenting. The buffer is
            // empty in a shipped game (nothing submits), so this is a no-op there; debug/editor code
            // populates it via the Gizmos service.
            if (gizmos != nullptr)
                gizmos->render(
                    backend,
                    camera_view.camera.get_view_projection_matrix(
                        camera_view.position, camera_view.rotation));

            invoke_pre_present_callback(backend, output_target, output_size);
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
        if (!asset_manager)
        {
            clear_swapchain(backend, Color::BLACK, output_size);
            return finish_frame();
        }

        // An explicit override world (e.g. the editor's isolated asset-preview view) renders in
        // place of the active world; otherwise the active world from the world manager is used as
        // before. active_world keeps the active world alive for the frame; the override's lifetime
        // is owned by the caller, held alive across the render dispatch.
        auto active_world = std::shared_ptr<World>();
        World* world = world_override;
        if (world == nullptr)
        {
            const auto world_manager = _world_manager.lock();
            if (!world_manager || !world_manager->has_active_world())
            {
                clear_swapchain(backend, Color::BLACK, output_size);
                return finish_frame();
            }
            active_world = world_manager->get_active_world().lock();
            world = active_world.get();
        }
        if (world == nullptr)
            return finish_frame();

        // Tags whose entities feed the tag mask this frame (from the active tag-gated post effects,
        // world and caller-supplied). Collected by capture() into view.mask_draw_commands.
        const auto masked_tags = _resources->post.masked_tags(*world, extra_post_effects);
        const WorldViewResult& view = _resources->view.capture(
            *asset_manager,
            *world,
            _resources->cache,
            camera_view,
            output_size,
            _elapsed_time,
            light_cull_distance,
            settings.shadow_render_distance,
            settings.shadow_softness,
            masked_tags);

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
        // Per-view local shadow matrices (one identity placeholder when no local light casts a shadow
        // so the SSBO always binds; the forward shader only indexes it for lights flagged shadowed).
        static const Mat4 identity_matrix(1.0F);
        const bool has_local_shadows = !view.local_shadow_matrices.empty();
        const GpuId local_shadow_matrices_buffer = frame.store(
            has_local_shadows ? view.local_shadow_matrices.data() : &identity_matrix,
            (has_local_shadows ? view.local_shadow_matrices.size() : 1U) * sizeof(Mat4),
            BufferUsage::STORAGE);
        if (instances_buffer == INVALID_GPU_ID || lights_buffer == INVALID_GPU_ID
            || uniforms_buffer == INVALID_GPU_ID || draw_args_buffer == INVALID_GPU_ID
            || local_shadow_matrices_buffer == INVALID_GPU_ID)
            return fail_frame(Result(false, "Failed to upload per-frame buffers."));

        GpuResource world_group = {};
        if (auto result = build_world_bind_group(
                backend,
                _resources->cache,
                instances_buffer,
                lights_buffer,
                uniforms_buffer,
                local_shadow_matrices_buffer,
                _backend,
                world_group);
            !result)
            return fail_frame(result);

        // When post-processing is active the forward pass renders into an offscreen scene target
        // the effect chain consumes; otherwise it renders straight to the swapchain (unchanged
        // path).
        const bool use_post = _resources->post.wants_post(*world, extra_post_effects);
        if (use_post)
        {
            if (auto result = _resources->post.ensure_targets(output_size); !result)
                return fail_frame(result);
        }

        constexpr uint32 stride = static_cast<uint32>(sizeof(GpuIndexedDrawCommand));

        //// SHADOW PASS ////
        // Two families of shadow maps feed the forward pass:
        //  - Cascaded directional shadows: one opaque depth sub-pass per cascade (each its own
        //    camera-centered ortho box + decreasing resolution; cascade 0 sharp/near, the furthest
        //    low-res/far) plus one translucent sub-pass per cascade that multiplies transparent
        //    casters' tint into that cascade's white-cleared color map, so glass casts a colored,
        //    partial shadow at every distance.
        //  - Local light shadows: one opaque depth sub-pass per view into a depth texture-array
        //    atlas (a spot/area light owns one layer, a point light six cube faces), so point/spot/
        //    area lights stop bleeding through walls.
        // The sky and ShadowMode::OFF materials were already excluded by world_view. Each caster
        // sub-pass reads its view's matrix from a small per-view UBO. The forward pass samples the
        // cascade depth maps (slots 12..) + color maps (slots 16..) + the local atlas (slot 20);
        // shadow_sample_group is null when nothing casts a shadow this frame.
        const bool has_directional_shadows = view.uniforms.shadow_count > 0U;
        GpuResource shadow_sample_group = {};
        if (has_directional_shadows || has_local_shadows)
        {
            // shadow_map_resolution is a Clamp<uint32, 256> so it can never drop below the floor the
            // cascade atlas needs — read it straight through.
            const uint32 base_resolution = settings.shadow_map_resolution;
            if (has_directional_shadows
                && (base_resolution != _resources->shadow_base_resolution
                    || !_resources->shadow_cascades[0].is_valid()))
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

                    // Matching colored transmittance map per cascade: same resolution + projection
                    // as the depth map so it registers exactly, and so colored glass shadows stay
                    // sharp near and reach far together with the depth cascades.
                    auto color_desc = TextureDesc {
                        .usage = TextureUsage::SAMPLED_RENDER_TARGET,
                        .format = TextureFormat::RGBA8,
                        .size = Size {res, res},
                        .is_linear_filtering_enabled = false};
                    auto color_id = INVALID_GPU_ID;
                    if (auto result = backend.create_texture(color_desc, color_id); !result)
                        return fail_frame(result);
                    _resources->shadow_color_maps[c] = GpuResource(_backend, color_id);
                }

                _resources->shadow_base_resolution = base_resolution;
            }

            // Local shadow atlas: one DEPTH32 texture array of MAX_LOCAL_SHADOW_VIEWS layers,
            // recreated only when the derived per-layer resolution changes.
            const uint32 local_resolution = local_shadow_resolution(base_resolution);
            if (has_local_shadows
                && (local_resolution != _resources->local_shadow_resolution
                    || !_resources->local_shadow_atlas.is_valid()))
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
                    return fail_frame(result);
                _resources->local_shadow_atlas = GpuResource(_backend, atlas_id);
                _resources->local_shadow_resolution = local_resolution;
            }

            // Upload the caster commands (concatenated in ShadowCasterCategory order). Empty means
            // a caster light exists but no geometry casts this frame — the passes still clear the
            // maps.
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
                return _resources->cache.add_pipeline(hash(program, state), program, state, true)
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

            // Uploads one caster view matrix as a per-view UBO and wraps it in a bind group the
            // caster vertex shaders read at GPU_BINDING_SHADOW_PASS_UNIFORMS. Used for both cascade
            // and local-light views.
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
                out_group = GpuResource(_backend, group_id);
                return Result::OK;
            };

            // Binds the category's pipeline + world group + the given view's matrix group and issues
            // its indirect draw. Skips empty categories and ones whose pipeline failed to compile
            // (the map simply stays cleared).
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
                        return fail_frame(result);

                // (1) Opaque depth sub-pass, one per cascade.
                for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
                {
                    const Size cascade_size {
                        _resources->shadow_cascade_sizes[c],
                        _resources->shadow_cascade_sizes[c]};
                    auto depth_pass = RenderPassDesc {
                        .depth_stencil_target = _resources->shadow_cascades[c].get(),
                        .clear_depth = 1.0F,
                        .clear_flags = ClearFlags::DEPTH,
                        .is_color_write_enabled = false};
                    depth_pass.viewport.dimensions = cascade_size;
                    if (auto result = backend.begin_render_pass(depth_pass); !result)
                        return fail_frame(result);
                    if (auto result = draw_category(
                            SHADOW_CASTER_OPAQUE_ONE_SIDED,
                            cascade_matrix_groups[c].get());
                        !result)
                        return fail_frame(result);
                    if (auto result = draw_category(
                            SHADOW_CASTER_OPAQUE_TWO_SIDED,
                            cascade_matrix_groups[c].get());
                        !result)
                        return fail_frame(result);
                    if (auto result = backend.end_render_pass(); !result)
                        return fail_frame(result);
                }

                // (2) Translucent transmittance sub-pass, one per cascade (its own projection). Each
                // clears its color map to white (fully transmissive) but keeps that cascade's opaque
                // depth so transparent casters behind opaque geometry are depth-rejected. Mirrors the
                // per-cascade depth passes so colored glass shadows are sharp near and reach far.
                for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
                {
                    const GpuId color_matrix_group = cascade_matrix_groups[c].get();
                    const Size color_size {
                        _resources->shadow_cascade_sizes[c],
                        _resources->shadow_cascade_sizes[c]};
                    auto color_pass = RenderPassDesc {
                        .color_targets = {_resources->shadow_color_maps[c].get()},
                        .depth_stencil_target = _resources->shadow_cascades[c].get(),
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
                }
            }

            if (has_local_shadows)
            {
                // One opaque depth sub-pass per local shadow view, each rendering the casters into
                // its own atlas layer through that view's matrix. Only opaque casters block local
                // lights (transparent casters are skipped — glass barely occludes a lamp), so no
                // colored transmittance pass is needed here.
                const Size atlas_size {
                    _resources->local_shadow_resolution,
                    _resources->local_shadow_resolution};
                for (uint32 layer = 0U; layer < view.local_shadow_matrices.size(); ++layer)
                {
                    GpuResource view_matrix_group = {};
                    if (auto result =
                            make_matrix_group(view.local_shadow_matrices[layer], view_matrix_group);
                        !result)
                        return fail_frame(result);

                    auto depth_pass = RenderPassDesc {
                        .depth_stencil_target = _resources->local_shadow_atlas.get(),
                        .depth_stencil_layer = static_cast<int32>(layer),
                        .clear_depth = 1.0F,
                        .clear_flags = ClearFlags::DEPTH,
                        .is_color_write_enabled = false};
                    depth_pass.viewport.dimensions = atlas_size;
                    if (auto result = backend.begin_render_pass(depth_pass); !result)
                        return fail_frame(result);
                    if (auto result =
                            draw_category(SHADOW_CASTER_OPAQUE_ONE_SIDED, view_matrix_group.get());
                        !result)
                        return fail_frame(result);
                    if (auto result =
                            draw_category(SHADOW_CASTER_OPAQUE_TWO_SIDED, view_matrix_group.get());
                        !result)
                        return fail_frame(result);
                    if (auto result = backend.end_render_pass(); !result)
                        return fail_frame(result);
                }
            }

            // Sample group for the forward pass: the cascade depth + colored transmittance maps (when
            // a directional caster is active) and the local light shadow atlas (when any local light
            // casts). Each binding lands on its consecutive slot; absent families simply aren't bound.
            auto sample_desc = BindGroupDesc {};
            if (has_directional_shadows)
            {
                for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
                    sample_desc.bindings.push_back(
                        ResourceBinding {
                            .binding_slot = GPU_BINDING_SHADOW_CASCADE_BASE + c,
                            .resource_handle = _resources->shadow_cascades[c].get()});
                for (uint32 c = 0U; c < SHADOW_CASCADE_COUNT; ++c)
                    sample_desc.bindings.push_back(
                        ResourceBinding {
                            .binding_slot = GPU_BINDING_SHADOW_COLOR_BASE + c,
                            .resource_handle = _resources->shadow_color_maps[c].get()});
            }
            if (has_local_shadows)
                sample_desc.bindings.push_back(
                    ResourceBinding {
                        .binding_slot = GPU_BINDING_LOCAL_SHADOW_ATLAS,
                        .resource_handle = _resources->local_shadow_atlas.get()});
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

        //// TAG MASK ////
        // Draw the tag-masked entities flat (white) into the tag mask a tag-gated post effect samples.
        // Always cleared so a stale mask never lingers; the silhouette is drawn only when something is
        // tagged. Runs only on the offscreen (post) path, where the mask target exists.
        if (use_post)
        {
            auto mask_pass = RenderPassDesc {
                .clear_color = Color(0.0F, 0.0F, 0.0F, 0.0F),
                .clear_flags = ClearFlags::COLOR};
            mask_pass.color_targets = {_resources->post.get_tag_mask()};
            mask_pass.viewport.dimensions = output_size;
            if (auto result = backend.begin_render_pass(mask_pass); !result)
                return fail_frame(result);

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
                    _resources->cache
                        .add_pipeline(hash(mask_program, mask_state), mask_program, mask_state, true)
                        .value_or(INVALID_GPU_ID);
                if (mask_args != INVALID_GPU_ID && mask_pipeline != INVALID_GPU_ID)
                {
                    if (auto result = backend.bind_raster_pipeline(mask_pipeline); !result)
                        return fail_frame(result);
                    if (auto result = backend.bind_group(0U, world_group.get()); !result)
                        return fail_frame(result);
                    if (auto result = backend.draw_indirect(
                            mask_args,
                            0U,
                            static_cast<uint32>(view.mask_draw_commands.size()),
                            stride);
                        !result)
                        return fail_frame(result);
                }
            }

            if (auto result = backend.end_render_pass(); !result)
                return fail_frame(result);
        }

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
                    uniforms_buffer,
                    extra_post_effects);
                !result)
                return fail_frame(result);
        }

        return finish_frame();
    }

    void RenderingPipeline::set_pre_present_callback(
        std::function<
            void(IGraphicsBackend& backend, const RenderTarget& output_target, const Size& backbuffer_size)>
            callback)
    {
        auto lock = std::lock_guard(_pre_present_mutex);
        _pre_present_callback = std::move(callback);
    }

    void RenderingPipeline::invoke_pre_present_callback(
        IGraphicsBackend& backend,
        const RenderTarget& output_target,
        const Size& backbuffer_size)
    {
        auto lock = std::lock_guard(_pre_present_mutex);
        if (_pre_present_callback)
            _pre_present_callback(backend, output_target, backbuffer_size);
    }

    void RenderingPipeline::reload()
    {
        // Recreate the cache + world view so reloaded shader sources recompile and caches rebuild.
        _resources = std::make_unique<Resources>(_backend, _asset_manager);
    }
}
