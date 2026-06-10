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

        // Persistent directional shadow map (depth texture) reused across frames; recreated only
        // when the configured resolution changes.
        GpuResource shadow_map = {};
        Size shadow_map_size = {0U, 0U};
    };

    //// STATIC HELPERS ////

    static const Color SKY_COLOR = Color(0.45F, 0.62F, 0.86F, 1.0F);

    // The depth-only caster pipeline (loaded shadow shaders + state) is pinned under this key.
    static constexpr CacheId SHADOW_PIPELINE_CACHE_ID = 0xB0FFE40000000010ULL;
    static const Handle SHADOW_VERTEX_SHADER_HANDLE = Handle("Shaders/Material/ShadowDepth.vert");
    static const Handle SHADOW_FRAGMENT_SHADER_HANDLE = Handle("Shaders/Material/ShadowDepth.frag");

    static ShaderProgram shadow_shader_program()
    {
        auto program = ShaderProgram {};
        program.vertex = SHADOW_VERTEX_SHADER_HANDLE;
        program.fragment = SHADOW_FRAGMENT_SHADER_HANDLE;
        return program;
    }

    static RasterState shadow_raster_state()
    {
        // Two-sided so thin/back faces still write depth, depth write on, no blending. Acne is
        // handled by the slope-scaled bias in the shadow sampling shader, not polygon offset.
        return RasterState {
            .is_blending_enabled = false,
            .is_two_sided = true,
            .is_depth_test_enabled = true,
            .is_depth_write_enabled = true,
            .depth_function = MaterialDepthFunction::LESS};
    }

    static ResourceBinding storage_binding(uint32 slot, GpuId buffer)
    {
        return ResourceBinding {.binding_slot = slot, .resource_handle = buffer};
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
            light_cull_distance);

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
        // Render the scene's depth from the directional caster's point of view into a depth map the
        // forward pass samples for occlusion. Built only when world_view flagged a caster this frame.
        // shadow_sample_group binds that map (slot 12) for the forward pass; it stays null otherwise.
        GpuResource shadow_sample_group = {};
        if (view.uniforms.shadow_count > 0U)
        {
            const uint32 resolution = std::max(settings.shadow_map_resolution, 256U);
            const Size shadow_size {resolution, resolution};
            if (!_resources->shadow_map.is_valid()
                || _resources->shadow_map_size.width != shadow_size.width
                || _resources->shadow_map_size.height != shadow_size.height)
            {
                auto desc = TextureDesc {
                    .usage = TextureUsage::SAMPLED_DEPTH_STENCIL,
                    .format = TextureFormat::DEPTH32_FLOAT,
                    .size = shadow_size,
                    .is_depth_comparison_enabled = false,
                    .is_linear_filtering_enabled = false};
                auto shadow_id = INVALID_GPU_ID;
                if (auto result = backend.create_texture(desc, shadow_id); !result)
                    return fail_frame(result);
                _resources->shadow_map = GpuResource(_backend, shadow_id);
                _resources->shadow_map_size = shadow_size;
            }

            const auto shadow_pipeline = _resources->cache.add_pipeline(
                SHADOW_PIPELINE_CACHE_ID, shadow_shader_program(), shadow_raster_state(), true);
            if (shadow_pipeline.has_value())
            {
                auto shadow_pass = RenderPassDesc {
                    .depth_stencil_target = _resources->shadow_map.get(),
                    .clear_depth = 1.0F,
                    .clear_flags = ClearFlags::DEPTH,
                    .is_color_write_enabled = false};
                shadow_pass.viewport.dimensions = shadow_size;
                if (auto result = backend.begin_render_pass(shadow_pass); !result)
                    return fail_frame(result);
                // One pipeline draws every caster: bind it first (resets the VAO's index binding),
                // then the world group (which attaches the index buffer + uniforms), then draw all
                // buckets' commands in a single indirect batch.
                if (auto result = backend.bind_raster_pipeline(shadow_pipeline.value()); !result)
                    return fail_frame(result);
                if (auto result = backend.bind_group(0U, world_group.get()); !result)
                    return fail_frame(result);
                const auto total_commands = static_cast<uint32>(view.draw_commands.size());
                if (total_commands != 0U)
                    if (auto result =
                            backend.draw_indirect(draw_args_buffer, 0U, total_commands, stride);
                        !result)
                        return fail_frame(result);
                if (auto result = backend.end_render_pass(); !result)
                    return fail_frame(result);

                auto sample_desc = BindGroupDesc {
                    .bindings = {ResourceBinding {
                        .binding_slot = GPU_BINDING_SHADOW_MAP,
                        .resource_handle = _resources->shadow_map.get()}}};
                auto sample_group_id = INVALID_GPU_ID;
                if (auto result = backend.create_bind_group(sample_desc, sample_group_id); !result)
                    return fail_frame(result);
                shadow_sample_group = GpuResource(_backend, sample_group_id);
            }
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
