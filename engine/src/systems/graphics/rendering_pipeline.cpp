#include "tbx/systems/graphics/rendering_pipeline.h"
#include "systems/graphics/internal/rendering_pipeline_internal.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/settings.h"
#include <string>
#include <utility>
#include <vector>

namespace tbx
{
    RenderingPipeline::RenderingPipeline(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<EntityRegistry> entity_registry,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<IWindowManager> window_manager)
        : _entity_registry(std::move(entity_registry))
        , _asset_manager(asset_manager)
        , _window_manager(std::move(window_manager))
        , _resource_manager(std::move(backend), std::move(asset_manager))
    {
    }

    Result RenderingPipeline::execute(
        IGraphicsBackend& backend,
        const GraphicsSettings& settings,
        const DeltaTime& delta_time)
    {
        const uint frame_index = _frame_index++;
        _elapsed_time += static_cast<float>(delta_time.seconds);

        const auto entity_registry = _entity_registry.lock();
        if (!entity_registry)
            return Result(false, "Rendering pipeline setup failed: scene service unavailable.");

        const auto window_manager = _window_manager.lock();
        if (!window_manager)
            return Result(false, "Rendering pipeline setup failed: window manager unavailable.");

        const auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return Result(false, "Rendering pipeline setup failed: asset manager unavailable.");

        // 1.) Resolve the render target and begin.
        const auto render_target =
            internal::extract_render_target(*entity_registry, *window_manager);
        auto result = backend.begin_frame(render_target);
        if (!result)
            return result;

        // 2.) Extract render data from the scene.
        auto render_data = internal::RenderData();
        result = internal::extract_render_data(
            _resource_manager,
            *entity_registry,
            *window_manager,
            *asset_manager,
            settings,
            frame_index,
            delta_time,
            _elapsed_time,
            render_target,
            render_data);
        if (!result)
        {
            backend.end_frame();
            return result;
        }

        // 3.) Create gbuffer.
        result = internal::create_gbuffer(_resource_manager, render_data);
        if (!result)
        {
            backend.end_frame();
            return result;
        }

        // 4.) Upload frame data and append draw commands.
        _passes.clear();
        result = internal::create_passes(
            frame_index,
            render_data,
            settings.shadow_map_resolution.value,
            settings.shadow_render_distance.value,
            settings.shadow_softness.value,
            _resource_manager,
            _passes);
        if (!result)
        {
            backend.end_frame();
            return result;
        }

        // 5.) Draw.
        result = internal::execute_passes(backend, _passes);
        if (!result)
        {
            backend.end_frame();
            return result;
        }

        // 6.) Present.
        result = backend.present();
        if (!result)
        {
            backend.end_frame();
            return result;
        }

        // 7.) End frame.
        result = backend.end_frame();
        if (!result)
            return result;

        // 8.) Let the resource manager retire stale GPU resources.
        _resource_manager.update(delta_time);

        return true;
    }
}
