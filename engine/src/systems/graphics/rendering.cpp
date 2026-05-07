#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/pipeline/render_pipeline_config.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include <functional>
#include <utility>

namespace tbx
{
    Rendering::Rendering(
        IGraphicsBackend& backend,
        EntityRegistry& entity_registry,
        AssetManager& asset_manager,
        IWindowManager& window_manager,
        Window output_window,
        const GraphicsSettings& settings)
        : _backend(backend)
        , _entity_registry(entity_registry)
        , _window_manager(window_manager)
        , _output_window(std::move(output_window))
        , _requested_resolution(settings.resolution.value)
        , _resource_manager(std::make_unique<GraphicsResourceManager>(backend, asset_manager))
        , _pipeline(backend)
    {
        _initialization_result = backend.initialize(settings);

        auto pipeline_config = RenderPipelineConfig::standard();
        for (auto& operation : pipeline_config.operations)
            _pipeline.add_operation(std::move(operation));
    }

    Rendering::~Rendering() noexcept
    {
        release_pipeline();
    }

    void Rendering::render()
    {
        if (!_initialization_result)
        {
            TBX_TRACE_ERROR(
                "Toybox renderer initialization failed: {}",
                _initialization_result.get_report());
            return;
        }
        if (!_output_window.is_valid() || !_window_manager.get().is_open(_output_window))
            return;

        _render_frame += 1U;
        if (_resource_manager)
            _resource_manager->update();

        auto render_data = std::make_unique<RenderData>(FrameData {
            .backend = _backend,
            .resource_manager = *_resource_manager,
            .entity_registry = _entity_registry,
            .window_manager = _window_manager,
            .output_window = _output_window,
            .requested_resolution = _requested_resolution,
            .frame_index = _render_frame,
        });

        const auto abort_frame = [this](const Result& failure)
        {
            auto* render_data = _pipeline.get_render_data();
            auto& backend = _backend.get();
            if (render_data && render_data->frame.view_started)
                backend.end_view();
            if (render_data && render_data->frame.frame_started)
                backend.end_frame();
            if (render_data)
            {
                render_data->frame.view_started = false;
                render_data->frame.frame_started = false;
            }
            TBX_TRACE_WARNING("Toybox renderer frame submission failed: {}", failure.get_report());
        };

        if (const auto result = _pipeline.prepare(std::move(render_data)); !result)
        {
            abort_frame(result);
            return;
        }

        if (const auto result = _pipeline.execute(CancellationToken {}); !result)
        {
            abort_frame(result);
            return;
        }
    }

    void Rendering::release_pipeline()
    {
        _backend.get().wait_for_idle();
        _pipeline.release();

        if (_resource_manager)
            _resource_manager->unload_all();
    }
}
