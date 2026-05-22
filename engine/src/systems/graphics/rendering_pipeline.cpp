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

    // TODO: Ensure render pipeline respecs ALL graphics settings
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

        // 1.) Construct frame and begin.
        auto frame = internal::create_frame_data(
            frame_index,
            delta_time,
            _elapsed_time,
            settings.resolution,
            *entity_registry,
            *window_manager);
        auto result = backend.begin_frame(frame.target);
        if (!result)
            return result;

        // 2.) Setup viewport to render into.
        result = backend.set_viewport(frame.viewport);
        if (!result)
        {
            backend.end_frame();
            return result;
        }

        // TODO: Update to follow result based architecture
        // 3.) Extract CPU-side render data from the scene.
        auto draw_data = internal::create_draw_data(
            *entity_registry,
            frame.camera.position,
            *asset_manager,
            settings.local_light_max_distance,
            settings.shadow_caster_max_distance);

        // 4.) Create shadow targets before scene targets so depth resources are ready for both
        // shadow rendering and lighting.
        result = internal::create_shadow_map(_resource_manager, settings, draw_data);
        if (!result)
        {
            backend.end_frame();
            return result;
        }

        // 5.) Create gbuffer.
        auto gbuffer = internal::GBuffer();
        result = internal::create_gbuffer(_resource_manager, frame, gbuffer);
        if (!result)
        {
            backend.end_frame();
            return result;
        }

        // TODO: Optimize shader pipeline we shouldn't have so many individual uploads, we should
        // utilize UBOs and upload everything at once where possible.
        // utilize other modern features to maximize perf such as making one shadow pass using
        // layered rendering via gl_Layer from a geometry shader or similar backend feature
        // extending GraphicsPassDesc/draw commands to support rendering to multiple depth array
        // layers in one pass changing shadow shaders/pipelines to emit the target layer

        // 6.) Upload frame data and append draw commands.
        auto passes = internal::create_passes(
            frame_index,
            frame,
            gbuffer,
            draw_data,
            settings,
            _resource_manager,
            backend);

        // 7.) Draw.
        result = internal::execute_passes(backend, passes);
        if (!result)
        {
            backend.end_frame();
            return result;
        }

        // 8.) Present.
        result = backend.present();
        if (!result)
        {
            backend.end_frame();
            return result;
        }

        // 9.) End frame.
        result = backend.end_frame();
        if (!result)
            return result;

        // 10.) Let the resource manager retire stale GPU resources.
        _resource_manager.update(delta_time);

        return true;
    }
}
