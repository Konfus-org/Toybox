#include "tbx/systems/graphics/rendering_pipeline.h"
#include "rendering_pipeline/passes/forward_pass.h"
#include "rendering_pipeline/passes/post_pass.h"
#include "rendering_pipeline/passes/shadow_pass.h"
#include "rendering_pipeline/pipeline_internal.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/frame_pass_context.h"
#include "tbx/types/color.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>
#include <vector>

namespace tbx
{
    // Whether the debug view applies to this camera: an empty tag gate applies everywhere;
    // otherwise any shared tag matches (the same any-of semantic tag-gated render passes use).
    static bool debug_view_applies(const RenderDebugView& debug_view, const CameraView& camera_view)
    {
        if (debug_view.camera_tags.empty())
            return true;
        for (const auto& tag : debug_view.camera_tags)
            if (std::ranges::find(camera_view.tags, tag) != camera_view.tags.end())
                return true;
        return false;
    }

    void RenderingPipeline::build_passes()
    {
        // The pipeline's built-in passes, each in its own file. They share the pipeline's resources by
        // reference and run merged with the camera-matched caller passes (ordered by PassType) each
        // frame.
        _passes.clear();
        _passes.push_back(std::make_unique<ShadowPass>(*_resources, _backend));
        _passes.push_back(std::make_unique<ForwardPass>(*_resources, _backend));
        _passes.push_back(std::make_unique<PostPass>(*_resources, _backend, _asset_manager));
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
        , _resources(std::make_unique<PipelineResources>(_backend, _asset_manager))
    {
        build_passes();
    }

    RenderingPipeline::RenderingPipeline(
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<IWindowManager> window_manager,
        std::weak_ptr<WorldManager> world_manager)
        : _asset_manager(std::move(asset_manager))
        , _window_manager(std::move(window_manager))
        , _world_manager(std::move(world_manager))
        , _resources(std::make_unique<PipelineResources>(_backend, _asset_manager))
    {
        build_passes();
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
        const std::vector<std::shared_ptr<RenderPass>>& caller_passes,
        World* world_override,
        uint64 frame_epoch)
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
            [this, &backend, &output_target, &output_size]() -> Result
        {
            // Overlays (e.g. the editor gizmos) are now overlay passes run in the execute phase before
            // this, so the back buffer already holds the finished frame; just present it.
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

        // The pass context the matching passes share this frame. Its world + GPU handles are filled in
        // by prepare_frame below; passes' prepare runs first (it only contributes CPU data such as post
        // effects). The engine-internal per-frame GPU state lives on _resources, reset here.
        _resources->frame_view = nullptr;
        _resources->frame_world_group = {};
        _resources->frame_shadow_sample_group = {};
        auto context = FramePassContext {
            .backend = backend,
            .camera_view = camera_view,
            .settings = settings,
            .output_size = output_size,
            .frame_epoch = frame_epoch,
            .frame_index = _frame_index};

        // The post effects the caller passes carry (each pass was already camera-matched by the
        // Rendering service), gathered before prepare_frame so a tag-gated effect's entities feed
        // this frame's tag mask.
        for (const auto& pass : caller_passes)
            context.extra_post_effects.insert(
                context.extra_post_effects.end(),
                pass->post_effects.begin(),
                pass->post_effects.end());

        // This frame's passes: the pipeline's built-ins plus the caller passes that matched this camera,
        // ordered by type (scene → post → overlay) but keeping registration order within each type. The
        // caller passes are kept alive across the render lane by the shared_ptrs the dispatch captured.
        auto run_passes = std::vector<RenderPass*>();
        run_passes.reserve(_passes.size() + caller_passes.size());
        for (const auto& pass : _passes)
            run_passes.push_back(pass.get());
        for (const auto& pass : caller_passes)
            run_passes.push_back(pass.get());
        std::stable_sort(
            run_passes.begin(),
            run_passes.end(),
            [](const RenderPass* a, const RenderPass* b) { return a->type < b->type; });

        // Per-phase CPU timing (steady_clock deltas in ns). Cheap enough to leave always on; the
        // pipeline logs an aggregate periodically so a slowdown can be attributed to scene capture vs
        // pass submission without an external profiler.
        using ProfileClock = std::chrono::steady_clock;
        const auto elapsed_ns = [](ProfileClock::time_point from, ProfileClock::time_point to) -> uint64
        { return static_cast<uint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(to - from).count()); };

        // Shared per-frame setup (world resolve, capture, transient buffers, world bind group) that the
        // passes consume. Soft outcomes (no world / nothing visible) clear-and-present; a GPU setup
        // failure fails the frame.
        auto setup_failure = Result();
        const auto prepare_start = ProfileClock::now();
        const auto readiness =
            prepare_frame(context, delta_time, light_cull_distance, world_override, setup_failure);
        const auto prepare_ns = elapsed_ns(prepare_start, ProfileClock::now());
        switch (readiness)
        {
            case FrameReadiness::ClearBlack:
                clear_swapchain(backend, Color::BLACK, output_size);
                return finish_frame();
            case FrameReadiness::ClearSky:
                clear_swapchain(backend, SKY_COLOR, output_size);
                return finish_frame();
            case FrameReadiness::Failed:
                return fail_frame(setup_failure);
            case FrameReadiness::Ready:
                break;
        }

        // Prepare phase: now that the scene is captured, each matching pass sets up its own GPU state
        // (the view is available here, so a pass can size resources to what it will draw). Runs before
        // any execute so a pass that produces a resource another consumes is ready in time.
        const auto pass_prepare_start = ProfileClock::now();
        for (RenderPass* pass : run_passes)
            if (auto result = pass->prepare(context); !result)
                return fail_frame(result);
        const auto pass_prepare_ns = elapsed_ns(pass_prepare_start, ProfileClock::now());

        // Execute phase: run each matching pass's GPU work in order (overlay passes last). The built-in
        // shadow pass fills the sample group the forward pass reads; the tag-mask/post passes no-op off
        // the offscreen path; an editor gizmo overlay draws on top last. A failing pass fails the whole
        // frame (magenta) rather than tearing down mid-pass.
        const auto pass_execute_start = ProfileClock::now();
        for (RenderPass* pass : run_passes)
            if (auto result = pass->execute(context); !result)
                return fail_frame(result);
        const auto pass_execute_ns = elapsed_ns(pass_execute_start, ProfileClock::now());

        const auto present_start = ProfileClock::now();
        const auto present_result = finish_frame();
        record_phase_timings(
            prepare_ns, pass_prepare_ns, pass_execute_ns, elapsed_ns(present_start, ProfileClock::now()));
        return present_result;
    }

    void RenderingPipeline::record_phase_timings(
        uint64 prepare_ns, uint64 pass_prepare_ns, uint64 pass_execute_ns, uint64 present_ns)
    {
        auto& t = _phase_timings;
        t.prepare_ns += prepare_ns;
        t.pass_prepare_ns += pass_prepare_ns;
        t.pass_execute_ns += pass_execute_ns;
        t.present_ns += present_ns;
        ++t.view_count;

        // Log an aggregate every so-many views, then reset the window. Views (not app-frames) so the
        // cadence is steady whether one camera or several editor viewports are rendering.
        constexpr uint64 LOG_EVERY_VIEWS = 300U;
        if (t.view_count < LOG_EVERY_VIEWS)
            return;

        const auto avg_ms = [&t](uint64 total_ns) -> double
        { return static_cast<double>(total_ns) / static_cast<double>(t.view_count) / 1.0e6; };
        TBX_TRACE_INFO(
            "Render phases (avg/view over {} views): capture {:.3f}ms | pass-prepare {:.3f}ms | "
            "pass-execute {:.3f}ms | present {:.3f}ms",
            t.view_count,
            avg_ms(t.prepare_ns),
            avg_ms(t.pass_prepare_ns),
            avg_ms(t.pass_execute_ns),
            avg_ms(t.present_ns));
        t = PhaseTimings {};
    }

    RenderingPipeline::FrameReadiness RenderingPipeline::prepare_frame(
        FramePassContext& context,
        const DeltaTime& delta_time,
        float light_cull_distance,
        World* world_override,
        Result& out_failure)
    {
        const auto backend_service = _backend.lock();
        if (!backend_service)
        {
            out_failure = Result(false, "Rendering pipeline has no graphics backend.");
            return FrameReadiness::Failed;
        }
        IGraphicsBackend& backend = *backend_service;

        _resources->cache.update(delta_time);

        const auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return FrameReadiness::ClearBlack;

        // An explicit override world (e.g. the editor's isolated asset-preview view) renders in place
        // of the active world; otherwise the active world from the world manager is used. active_world
        // keeps the active world alive for the frame; the override's lifetime is owned by the caller.
        auto active_world = std::shared_ptr<World>();
        World* world = world_override;
        if (world == nullptr)
        {
            const auto world_manager = _world_manager.lock();
            if (!world_manager || !world_manager->has_active_world())
                return FrameReadiness::ClearBlack;
            active_world = world_manager->get_active_world().lock();
            world = active_world.get();
        }
        if (world == nullptr)
            return FrameReadiness::ClearBlack;
        context.world = world;

// Tags whose entities feed the tag mask this frame: the world's own tag-gated post effects
        // plus this camera's injected extras. Collected by capture() into view.mask_draw_commands.
        const auto masked_tags = PostProcessor::masked_tags(*world, context.extra_post_effects);
        const WorldViewResult& view = _resources->view.capture(
            *asset_manager,
            *world,
            _resources->cache,
            context.camera_view,
            context.output_size,
            _elapsed_time,
            light_cull_distance,
            context.settings.shadow_render_distance,
            context.settings.shadow_softness,
            context.settings.min_screen_size,
            context.settings.screen_size_fade_fraction,
            masked_tags);
        if (!view.has_camera || view.instances.empty())
            return FrameReadiness::ClearSky;
        _resources->frame_view = &view;

        // The editor's debug view (render-stage override / post kill), applied only to cameras its
        // tag gate matches so a game view keeps rendering normally beside a debugging editor view.
        auto debug_view = RenderDebugView();
        {
            auto lock = std::lock_guard(_debug_view_mutex);
            debug_view = _debug_view;
        }
        const bool debug_applies = debug_view_applies(debug_view, context.camera_view);

        // Upload this frame's transient buffers into the reusable pool: begin() rewinds the slot
        // cursor, and the buffers persist across frames (the steady state recreates none of them).
        _resources->frame_buffers.begin();
        FrameBuffers& frame = _resources->frame_buffers;
        const GpuId instances_buffer = frame.store(
            view.instances.data(),
            view.instances.size() * sizeof(GpuInstanceData),
            BufferUsage::STORAGE);
        const GpuId lights_buffer = frame.store(
            view.lights.data(),
            view.lights.size() * sizeof(GpuLightData),
            BufferUsage::STORAGE);
        // The captured uniforms are shared frame state; the debug stage is per-camera, so patch a
        // copy rather than the capture.
        auto uniforms = view.uniforms;
        uniforms.debug_stage = debug_applies ? static_cast<uint32>(debug_view.stage) : 0U;
        const GpuId uniforms_buffer =
            frame.store(&uniforms, sizeof(GpuUniforms), BufferUsage::UNIFORM);
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
        {
            out_failure = Result(false, "Failed to upload per-frame buffers.");
            return FrameReadiness::Failed;
        }

        if (auto result = build_world_bind_group(
                backend,
                _resources->cache,
                instances_buffer,
                lights_buffer,
                uniforms_buffer,
                local_shadow_matrices_buffer,
                _backend,
                _resources->frame_world_group);
            !result)
        {
            out_failure = result;
            return FrameReadiness::Failed;
        }

        // When post-processing is active the forward pass renders into an offscreen scene target the
        // effect chain consumes; otherwise it renders straight to the swapchain. The post pass owns
        // creating those targets — it does so in its prepare (which runs before the forward execute).
        context.draw_args_buffer = draw_args_buffer;
        context.uniforms_buffer = uniforms_buffer;
        context.stride = static_cast<uint32>(sizeof(GpuIndexedDrawCommand));
        // The debug view can kill post for its cameras: explicitly (the toggle) or implicitly (a
        // non-final stage outputs raw intermediates that post effects would only distort).
        context.use_post = PostProcessor::wants_post(*world, context.extra_post_effects)
                           && (!debug_applies
                               || (debug_view.post_processing_enabled
                                   && debug_view.stage == RenderDebugStage::FINAL));
        return FrameReadiness::Ready;
    }

    void RenderingPipeline::set_debug_view(RenderDebugView debug_view)
    {
        auto lock = std::lock_guard(_debug_view_mutex);
        _debug_view = std::move(debug_view);
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
        _resources = std::make_unique<PipelineResources>(_backend, _asset_manager);
    }
}
