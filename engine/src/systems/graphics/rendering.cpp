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
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<EntityRegistry> entity_registry,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<ThreadManager> thread_manager,
        std::weak_ptr<IWindowManager> window_manager,
        Window output_window,
        const GraphicsSettings& settings)
        : _thread_manager(std::move(thread_manager))
        , _backend(std::move(backend))
        , _entity_registry(std::move(entity_registry))
        , _window_manager(std::move(window_manager))
        , _output_window(std::move(output_window))
        , _requested_resolution(settings.resolution.value)
        , _shadow_map_resolution(settings.shadow_map_resolution.value)
        , _shadow_render_distance(settings.shadow_render_distance.value)
        , _shadow_softness(settings.shadow_softness.value)
        , _local_light_max_distance(settings.local_light_max_distance.value)
        , _shadow_caster_max_distance(settings.shadow_caster_max_distance.value)
        , _resource_manager(nullptr)
        , _pipeline(_backend)
    {
        auto backend_strong = _backend.lock();
        auto asset_manager_strong = asset_manager.lock();
        auto thread_manager_strong = _thread_manager.lock();
        if (!backend_strong || !asset_manager_strong || !thread_manager_strong)
        {
            _initialization_result.flag_failure(
                "Rendering requires graphics, assets, and thread services.");
            return;
        }

        _resource_manager =
            std::make_unique<GraphicsResourceManager>(*backend_strong, *asset_manager_strong);

        thread_manager_strong->try_create_lane(RENDER_LANE_NAME);
        if (!thread_manager_strong->has_lane(RENDER_LANE_NAME))
        {
            _initialization_result.flag_failure("Rendering could not acquire the render lane.");
            return;
        }

        try
        {
            _initialization_future = thread_manager_strong->post_with_future(
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
            auto thread_manager = _thread_manager.lock();
            wait_for_render_frame();
            wait_for_initialization();
            if (thread_manager && thread_manager->has_lane(RENDER_LANE_NAME))
            {
                auto release_future = thread_manager->post_with_future(
                    RENDER_LANE_NAME,
                    [this]()
                    {
                        release_pipeline();
                    });
                release_future.get();
                thread_manager->stop_lane(RENDER_LANE_NAME);
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
        auto thread_manager = _thread_manager.lock();
        if (!thread_manager || !thread_manager->has_lane(RENDER_LANE_NAME))
        {
            TBX_TRACE_ERROR("Toybox renderer render lane is unavailable.");
            return;
        }

        wait_for_initialization();
        wait_for_render_frame();

        try
        {
            _render_future = thread_manager->post_with_future(
                RENDER_LANE_NAME,
                [this]()
                {
                    render_frame();
                });

            // EntityRegistry is not thread-safe, so frame updates must not overlap render-side
            // ECS reads. Waiting here keeps render submission deterministic relative to update.
            wait_for_render_frame();
        }
        catch (const std::exception& ex)
        {
            TBX_TRACE_ERROR("Toybox renderer dispatch failed: {}", ex.what());
        }
    }

    void Rendering::initialize(const GraphicsSettings& settings)
    {
        auto backend = _backend.lock();
        auto entity_registry = _entity_registry.lock();
        auto window_manager = _window_manager.lock();
        if (!backend || !entity_registry || !window_manager || !_resource_manager)
        {
            _initialization_result.flag_failure(
                "Rendering initialization failed because required services are unavailable.");
            return;
        }

        _initialization_result = backend->initialize(settings);

        auto pipeline_config = RenderPipelineConfig::standard(
            _backend,
            *_resource_manager,
            _entity_registry,
            _window_manager);
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
        auto window_manager = _window_manager.lock();
        if (!window_manager || !_output_window.is_valid() || !window_manager->is_open(_output_window))
            return;

        _render_frame += 1U;
        if (_resource_manager)
            _resource_manager->update();

        auto render_data = std::make_unique<RenderData>();
        render_data->output_window = _output_window;
        render_data->requested_resolution = _requested_resolution;
        render_data->frame_index = _render_frame;
        render_data->shadow_map_resolution = _shadow_map_resolution;
        render_data->shadow_render_distance = _shadow_render_distance;
        render_data->shadow_softness = _shadow_softness;
        render_data->local_light_max_distance = _local_light_max_distance;
        render_data->shadow_caster_max_distance = _shadow_caster_max_distance;

        const auto abort_frame = [this](const Result& failure)
        {
            auto* render_data = _pipeline.get_render_data();
            if (auto backend = _backend.lock())
            {
                if (render_data && render_data->view_started)
                    backend->end_view();
                if (render_data && render_data->frame_started)
                    backend->end_frame();
            }
            if (render_data)
            {
                render_data->view_started = false;
                render_data->frame_started = false;
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
        if (auto backend = _backend.lock())
            backend->wait_for_idle();
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
