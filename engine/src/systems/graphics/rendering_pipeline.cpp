#include "tbx/systems/graphics/rendering_pipeline.h"

#include "rendering_pipeline/render_pipeline_stages.h"
#include <memory>
#include <utility>

namespace tbx
{
    struct RenderingPipeline::State final
    {
        explicit State(std::weak_ptr<IGraphicsBackend> graphics_backend)
            : context(std::move(graphics_backend))
        {
        }

        RenderPipelineContext context;
        FrameUploadStage frame_upload_stage = {};
        VisibilityCullingStage visibility_culling_stage = {};
        GpuBarrierStage gpu_barrier_stage = {};
        ShadowAtlasStage shadow_atlas_stage = {};
        GBufferStage gbuffer_stage = {};
        LightingResolveStage lighting_resolve_stage = {};
        PresentStage present_stage = {};
    };

    RenderingPipeline::RenderingPipeline(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<IWindowManager> window_manager,
        std::weak_ptr<WorldManager> world_manager)
        : _backend(std::move(backend))
        , _asset_manager(std::move(asset_manager))
        , _window_manager(std::move(window_manager))
        , _world_manager(std::move(world_manager))
        , _state(std::make_unique<State>(_backend))
    {
    }

    RenderingPipeline::RenderingPipeline(
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<IWindowManager> window_manager,
        std::weak_ptr<WorldManager> world_manager)
        : _asset_manager(std::move(asset_manager))
        , _window_manager(std::move(window_manager))
        , _world_manager(std::move(world_manager))
        , _state(std::make_unique<State>(_backend))
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

    RenderingPipeline::~RenderingPipeline() = default;

    Result RenderingPipeline::execute(
        IGraphicsBackend& backend,
        const GraphicsSettings& settings,
        const DeltaTime& delta_time)
    {
        _elapsed_time += static_cast<float>(delta_time.seconds);

        const auto window_manager = _window_manager.lock();
        if (!window_manager || !window_manager->has_main_window())
            return Result(true);

        const Window output = window_manager->get_main_window();
        if (auto result = backend.begin_frame(output); !result)
            return result;

        const auto finish_frame = [&backend]() -> Result
        {
            const auto present_result = backend.present();
            const auto end_result = backend.end_frame();
            return present_result ? end_result : present_result;
        };
        const auto fail_frame = [&backend, this](const Result& result) -> Result
        {
            const auto failure_result = _state->present_stage.execute_failure_frame(backend);
            return failure_result ? result : failure_result;
        };

        const auto asset_manager = _asset_manager.lock();
        const auto world_manager = _world_manager.lock();
        if (!asset_manager || !world_manager || !world_manager->has_active_world())
            return finish_frame();

        const auto world = world_manager->get_active_world().lock();
        if (!world)
            return finish_frame();

        auto resources = UploadedFrameResources();
        if (auto result = _state->frame_upload_stage.execute(
                backend,
                _state->context,
                *asset_manager,
                *world,
                output,
                *window_manager,
                settings,
                _elapsed_time,
                resources);
            !result)
        {
            return fail_frame(result);
        }

        if (resources.instance_count == 0U)
            return finish_frame();

        if (auto result = _state->visibility_culling_stage.execute(
                backend,
                _state->context,
                *asset_manager,
                resources);
            !result)
        {
            return fail_frame(result);
        }

        if (auto result = _state->gpu_barrier_stage.execute(
                backend,
                resources.buffers,
                GpuBarrierPoint::VISIBILITY_TO_RASTER);
            !result)
        {
            return fail_frame(result);
        }

        if (auto result = _state->shadow_atlas_stage.execute(
                backend,
                _state->context,
                *asset_manager,
                resources);
            !result)
        {
            return fail_frame(result);
        }

        if (auto result = _state->gpu_barrier_stage.execute(
                backend,
                resources.buffers,
                GpuBarrierPoint::SHADOW_TO_GBUFFER);
            !result)
        {
            return fail_frame(result);
        }

        if (auto result = _state->gbuffer_stage.execute(
                backend,
                _state->context,
                *asset_manager,
                resources);
            !result)
        {
            return fail_frame(result);
        }

        if (auto result = _state->gpu_barrier_stage.execute(
                backend,
                resources.buffers,
                GpuBarrierPoint::GBUFFER_TO_LIGHTING);
            !result)
        {
            return fail_frame(result);
        }

        if (auto result = _state->lighting_resolve_stage.execute(
                backend,
                _state->context,
                *asset_manager,
                resources);
            !result)
        {
            return fail_frame(result);
        }

        if (auto result = _state->gpu_barrier_stage.execute(
                backend,
                resources.buffers,
                GpuBarrierPoint::LIGHTING_TO_PRESENT);
            !result)
        {
            return fail_frame(result);
        }

        if (auto result = _state->present_stage.execute(
                backend,
                _state->context,
                *asset_manager,
                resources);
            !result)
        {
            return fail_frame(result);
        }

        return finish_frame();
    }

    void RenderingPipeline::reload()
    {
        if (!_state)
            return;

        _state = std::make_unique<State>(_backend);
    }
}
