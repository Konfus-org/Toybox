#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/debugging/macros.h"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tbx
{
    constexpr auto RENDER_LANE_NAME = std::string_view("render");

    Rendering::Rendering(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<EntityRegistry> entity_registry,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<ThreadManager> thread_manager,
        std::weak_ptr<IWindowManager> window_manager,
        const GraphicsSettings& settings)
        : _thread_manager(std::move(thread_manager))
        , _backend(std::move(backend))
        , _pass_factory(
              _backend,
              std::move(entity_registry),
              std::move(asset_manager),
              std::move(window_manager),
              settings.resolution.value)
    {
        auto thread_manager_service = _thread_manager.lock();
        if (!thread_manager_service)
        {
            _initialization_result.flag_failure(
                "Rendering initialization failed because thread manager is unavailable.");
            return;
        }

        if (!thread_manager_service->has_lane(std::string(RENDER_LANE_NAME))
            && !thread_manager_service->try_create_lane(std::string(RENDER_LANE_NAME)))
        {
            _initialization_result.flag_failure(
                "Rendering initialization failed because render lane creation failed.");
            return;
        }

        _initialization_future = thread_manager_service->post_with_future(
            std::string(RENDER_LANE_NAME),
            [this, settings]()
            {
                initialize(settings);
            });
    }

    Rendering::~Rendering() noexcept
    {
        TBX_TRY_CATCH_ASSERT(
            {
                wait_for_initialization();
                wait_for_render_frame();

                if (auto thread_manager = _thread_manager.lock())
                {
                    if (auto backend = _backend.lock())
                    {
                        auto idle_future = thread_manager->post_with_future(
                            std::string(RENDER_LANE_NAME),
                            [backend]()
                            {
                                backend->wait_for_idle();
                            });
                        idle_future.get();
                    }

                    thread_manager->stop_lane(std::string(RENDER_LANE_NAME));
                }
            },
            "Toybox renderer shutdown failed.");
    }

    void Rendering::initialize(const GraphicsSettings& settings)
    {
        auto backend = _backend.lock();
        if (!backend)
        {
            _initialization_result.flag_failure(
                "Rendering initialization failed because required services are unavailable.");
            return;
        }

        _initialization_result = backend->initialize(settings);
    }

    void Rendering::render()
    {
        auto thread_manager = _thread_manager.lock();
        if (!thread_manager || !thread_manager->has_lane(std::string(RENDER_LANE_NAME)))
        {
            TBX_TRACE_ERROR("Toybox renderer render lane is unavailable.");
            return;
        }

        TBX_TRY_CATCH_ASSERT(
            {
                // Ensure we are initialized
                wait_for_initialization();

                _render_future = thread_manager->post_with_future(
                    std::string(RENDER_LANE_NAME),
                    [this]()
                    {
                        render_frame();
                    });
            },
            "Toybox renderer dispatch failed");
    }

    void Rendering::wait_for_pending_frame() noexcept
    {
        wait_for_render_frame();
    }

    void Rendering::render_frame()
    {
        if (!_initialization_result)
        {
            TBX_TRACE_ERROR_ONCE(
                "Toybox renderer initialization failed. {}",
                _initialization_result.get_report());
            return;
        }

        const auto backend = _backend.lock();
        if (!backend)
        {
            TBX_TRACE_ERROR_ONCE("Toybox renderer graphics backend is unavailable.");
            return;
        }

        const DeltaTime frame_delta = _frame_timer.tick();

        auto render_target = RenderTarget();
        auto view = RenderView();
        auto result = _pass_factory.build_frame_data(frame_delta, render_target, view);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE("Toybox render frame data creation failed. {}", result.get_report());
            return;
        }

        result = backend->begin_frame(render_target);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE("Toybox begin_frame failed. {}", result.get_report());
            return;
        }

        auto passes = std::vector<RenderPass>();
        result = _pass_factory.create(_frame_index, _resource_tracker, passes);
        if (!result)
        {
            (void)backend->end_frame();
            TBX_TRACE_ERROR_ONCE("Toybox render pass creation failed. {}", result.get_report());
            return;
        }

        result = backend->begin_view(view);
        if (!result)
        {
            (void)backend->end_frame();
            TBX_TRACE_ERROR_ONCE("Toybox begin_view failed. {}", result.get_report());
            return;
        }

        result = backend->set_viewport(view.viewport);
        if (!result)
        {
            (void)backend->end_view();
            (void)backend->end_frame();
            TBX_TRACE_ERROR_ONCE("Toybox set_viewport failed. {}", result.get_report());
            return;
        }

        result = _draw_command_executor.execute(*backend, passes);
        if (!result)
        {
            (void)backend->end_view();
            (void)backend->end_frame();
            TBX_TRACE_ERROR_ONCE("Toybox draw command execution failed. {}", result.get_report());
            return;
        }

        result = backend->end_view();
        if (!result)
        {
            (void)backend->end_frame();
            TBX_TRACE_ERROR_ONCE("Toybox end_view failed. {}", result.get_report());
            return;
        }

        result = backend->present();
        if (!result)
        {
            (void)backend->end_frame();
            TBX_TRACE_ERROR_ONCE("Toybox present failed. {}", result.get_report());
            return;
        }

        end_frame(*backend, frame_delta);
    }

    void Rendering::end_frame(IGraphicsBackend& backend, const DeltaTime delta_time)
    {
        auto result = backend.end_frame();
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE("Toybox end_frame failed. {}", result.get_report());
            return;
        }

        _resource_tracker.update(delta_time);
        unload_expired_resources(backend, 3.0F);
        _frame_index += 1U;
    }

    uint Rendering::unload_expired_resources(
        IGraphicsBackend& backend,
        const float max_time_alive_seconds)
    {
        auto resources = _resource_tracker.get_tracked_resources();
        auto unloaded_count = uint(0U);
        for (const uint resource : resources)
        {
            if (_resource_tracker.get_time_alive(resource) < max_time_alive_seconds)
                continue;

            const Result result = backend.unload(Uuid(resource));
            if (!result)
            {
                TBX_TRACE_ERROR_ONCE(
                    "Toybox resource unload failed for resource {}. {}",
                    resource,
                    result.get_report());
                continue;
            }

            _resource_tracker.untrack(resource);
            unloaded_count += 1U;
        }

        return unloaded_count;
    }

    void Rendering::wait_for_initialization() noexcept
    {
        if (!_initialization_future.valid())
            return;

        TBX_TRY_CATCH_ASSERT(_initialization_future.get();
                             , "Toybox renderer initialization completion failed.");
    }

    void Rendering::wait_for_render_frame() noexcept
    {
        if (!_render_future.valid())
            return;

        TBX_TRY_CATCH_ASSERT(_render_future.get();, "Toybox renderer frame completion failed.");
    }
}
