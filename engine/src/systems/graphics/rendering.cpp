#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/graphics/pipeline/render_pipeline_config.h"
#include <functional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace tbx
{
    constexpr auto RENDER_LANE_NAME = std::string_view("render");

    Rendering::Rendering(
        IGraphicsBackend& backend,
        EntityRegistry& entity_registry,
        AssetManager& asset_manager,
        ThreadManager& thread_manager,
        IWindowManager& window_manager,
        Window output_window,
        const GraphicsSettings& settings)
        : _thread_manager(thread_manager)
        , _backend(backend)
        , _entity_registry(entity_registry)
        , _window_manager(window_manager)
        , _output_window(std::move(output_window))
        , _requested_resolution(settings.resolution.value)
        , _shadow_map_resolution(settings.shadow_map_resolution.value)
        , _shadow_render_distance(settings.shadow_render_distance.value)
        , _shadow_softness(settings.shadow_softness.value)
        , _resource_manager(std::make_unique<GraphicsResourceManager>(backend, asset_manager))
        , _pipeline(backend)
    {
        _thread_manager.get().try_create_lane(RENDER_LANE_NAME);
        if (!_thread_manager.get().has_lane(RENDER_LANE_NAME))
        {
            _initialization_result.flag_failure("Rendering could not acquire the render lane.");
            return;
        }

        try
        {
            _initialization_future = _thread_manager.get().post_with_future(
                RENDER_LANE_NAME,
                [this, &settings]()
                {
                    initialize(settings);
                });
        }
        catch (const std::exception& ex)
        {
            _initialization_result.flag_failure(ex.what());
        }
    }

    Rendering::~Rendering() noexcept
    {
        try
        {
            wait_for_render_frame();
            wait_for_initialization();
            if (_thread_manager.get().has_lane(RENDER_LANE_NAME))
            {
                auto release_future = _thread_manager.get().post_with_future(
                    RENDER_LANE_NAME,
                    [this]()
                    {
                        release_pipeline();
                    });
                release_future.get();
                _thread_manager.get().stop_lane(RENDER_LANE_NAME);
                return;
            }
        }
        catch (const std::exception& ex)
        {
            TBX_TRACE_ERROR("Toybox renderer shutdown failed: {}", ex.what());
        }
        catch (...)
        {
            TBX_TRACE_ERROR("Toybox renderer shutdown failed with an unknown error.");
        }

        release_pipeline();
    }

    void Rendering::render()
    {
        if (!_thread_manager.get().has_lane(RENDER_LANE_NAME))
        {
            TBX_TRACE_ERROR("Toybox renderer render lane is unavailable.");
            return;
        }

        wait_for_initialization();
        wait_for_render_frame();

        try
        {
            _render_future = _thread_manager.get().post_with_future(
                RENDER_LANE_NAME,
                [this]()
                {
                    render_frame();
                });
        }
        catch (const std::exception& ex)
        {
            TBX_TRACE_ERROR("Toybox renderer dispatch failed: {}", ex.what());
        }
    }

    void Rendering::initialize(const GraphicsSettings& settings)
    {
        _initialization_result = _backend.get().initialize(settings);

        auto pipeline_config = RenderPipelineConfig::standard();
        for (auto& operation : pipeline_config.operations)
            _pipeline.add_operation(std::move(operation));
    }

    void Rendering::render_frame()
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
            .shadow_map_resolution = _shadow_map_resolution,
            .shadow_render_distance = _shadow_render_distance,
            .shadow_softness = _shadow_softness,
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
        else if (!result.get_report().empty())
        {
            TBX_TRACE_WARNING(
                "Toybox renderer recoverable prepare issues: {}",
                result.get_report());
        }

        if (const auto result = _pipeline.execute(CancellationToken {}); !result)
        {
            abort_frame(result);
            return;
        }
        else if (!result.get_report().empty())
        {
            TBX_TRACE_WARNING(
                "Toybox renderer recoverable execute issues: {}",
                result.get_report());
        }
    }

    void Rendering::release_pipeline()
    {
        _backend.get().wait_for_idle();
        _pipeline.release();

        if (_resource_manager)
            _resource_manager->unload_all();
    }

    void Rendering::wait_for_initialization() noexcept
    {
        if (!_initialization_future.valid())
            return;

        try
        {
            _initialization_future.get();
        }
        catch (const std::exception& ex)
        {
            TBX_TRACE_ERROR("Toybox renderer initialization completion failed: {}", ex.what());
        }
        catch (...)
        {
            TBX_TRACE_ERROR(
                "Toybox renderer initialization completion failed with an unknown error.");
        }
    }

    void Rendering::wait_for_render_frame() noexcept
    {
        if (!_render_future.valid())
            return;

        try
        {
            _render_future.get();
        }
        catch (const std::exception& ex)
        {
            TBX_TRACE_ERROR("Toybox renderer frame completion failed: {}", ex.what());
        }
        catch (...)
        {
            TBX_TRACE_ERROR("Toybox renderer frame completion failed with an unknown error.");
        }
    }
}
