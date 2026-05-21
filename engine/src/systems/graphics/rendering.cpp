#include "tbx/systems/graphics/rendering.h"
#include "systems/graphics/internal/rendering_internal.h"
#include "tbx/systems/debugging/macros.h"
#include <string>
#include <string_view>
#include <utility>
namespace tbx
{
    Rendering::Rendering(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<EntityRegistry> entity_registry,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<ThreadManager> thread_manager,
        std::weak_ptr<IWindowManager> window_manager,
        const GraphicsSettings& settings)
        : _thread_manager(std::move(thread_manager))
        , _backend(std::move(backend))
        , _window_manager(window_manager)
        , _pipeline(
              _backend,
              std::move(entity_registry),
              std::move(asset_manager),
              std::move(window_manager),
              settings)
    {
        auto thread_manager_service = _thread_manager.lock();
        if (!thread_manager_service)
        {
            _initialization_result.flag_failure(
                "Rendering initialization failed because thread manager is unavailable.");
            return;
        }

        if (!thread_manager_service->has_lane(std::string(internal::RENDER_LANE_NAME))
            && !thread_manager_service->try_create_lane(std::string(internal::RENDER_LANE_NAME)))
        {
            _initialization_result.flag_failure(
                "Rendering initialization failed because render lane creation failed.");
            return;
        }

        _initialization_future = thread_manager_service->post_with_future(
            std::string(internal::RENDER_LANE_NAME),
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
                            std::string(internal::RENDER_LANE_NAME),
                            [backend]()
                            {
                                backend->wait_for_idle();
                            });
                        idle_future.get();
                    }

                    thread_manager->stop_lane(std::string(internal::RENDER_LANE_NAME));
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

    void Rendering::render(const DeltaTime& delta_time)
    {
        auto thread_manager = _thread_manager.lock();
        if (!thread_manager || !thread_manager->has_lane(std::string(internal::RENDER_LANE_NAME)))
        {
            TBX_TRACE_ERROR("Toybox renderer render lane is unavailable.");
            return;
        }

        // Need at least the main window to render...
        // if no main window is open then nothing to render to
        const auto window_manager = _window_manager.lock();
        if (!window_manager || !window_manager->has_main_window())
            return;

        TBX_TRY_CATCH_ASSERT(
            {
                // Ensure we are initialized
                wait_for_initialization();

                _render_future = thread_manager->post_with_future(
                    std::string(internal::RENDER_LANE_NAME),
                    [this, delta_time]()
                    {
                        render_frame(delta_time);
                    });
            },
            "Toybox renderer dispatch failed");
    }

    void Rendering::wait_for_pending_frame() noexcept
    {
        wait_for_render_frame();
    }

    void Rendering::render_frame(const DeltaTime& delta_time)
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

        const auto result = _pipeline.execute(*backend, delta_time);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE(
                "Toybox rendering pipeline execution failed. {}",
                result.get_report());
        }
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
