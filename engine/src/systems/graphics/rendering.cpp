#include "tbx/systems/graphics/rendering.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/debugging/macros.h"
#include <tuple>

namespace tbx
{
    constexpr auto RENDER_LANE_NAME = std::string_view("render");

    Rendering::Rendering(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<ThreadManager> thread_manager,
        std::weak_ptr<IWindowManager> window_manager,
        std::weak_ptr<WorldManager> world_manager,
        std::weak_ptr<IMessageCoordinator> message_coordinator,
        Handle)
        : _thread_manager(std::move(thread_manager))
        , _message_coordinator(message_coordinator)
        , _backend(std::move(backend))
        , _window_manager(window_manager)
        , _pipeline(
              _backend,
              std::move(asset_manager),
              std::move(window_manager),
              std::move(world_manager))
    {
        if (auto coordinator = _message_coordinator.lock())
        {
            _asset_reload_handler = coordinator->register_handler(
                [this](Message& message)
                {
                    if (const auto reloaded = handle_message<AssetReloadedEvent>(message))
                        on_asset_reloaded(reloaded->get());
                });
        }

        auto thread_manager_service = _thread_manager.lock();
        if (!thread_manager_service)
        {
            TBX_TRACE_ERROR(
                "Rendering initialization failed because thread manager is unavailable.");
            return;
        }

        if (!thread_manager_service->has_lane(RENDER_LANE_NAME)
            && !thread_manager_service->try_create_lane(RENDER_LANE_NAME))
        {
            TBX_TRACE_ERROR("Rendering initialization failed because render lane creation failed.");
            return;
        }
    }

    Rendering::~Rendering() noexcept
    {
        TBX_TRY_CATCH_ASSERT(
            {
                wait_for_render_frame();
                if (auto coordinator = _message_coordinator.lock())
                    coordinator->deregister_handler(_asset_reload_handler);

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

    void Rendering::render(const DeltaTime& delta_time, const GraphicsSettings& settings)
    {
        auto thread_manager = _thread_manager.lock();
        if (!thread_manager || !thread_manager->has_lane(RENDER_LANE_NAME))
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
                _render_future = thread_manager->post_with_future(
                    RENDER_LANE_NAME,
                    [this, delta_time, settings]()
                    {
                        render_frame(delta_time, settings);
                    });
            },
            "Toybox renderer dispatch failed");
    }

    void Rendering::wait_for_pending_frame() noexcept
    {
        wait_for_render_frame();
    }

    void Rendering::on_asset_reloaded(const AssetReloadedEvent& event)
    {
        if (!event.succeeded || !event.affected_asset.id.is_valid())
            return;

        const auto thread_manager = _thread_manager.lock();
        const auto backend = _backend.lock();
        if (!thread_manager || !backend || !thread_manager->has_lane(RENDER_LANE_NAME))
            return;

        std::ignore = thread_manager->post_with_future(
            std::string(RENDER_LANE_NAME),
            [this, backend]()
            {
                if (!backend)
                    return;

                _pipeline.reload();
            });
    }

    void Rendering::render_frame(const DeltaTime& delta_time, const GraphicsSettings& settings)
    {
        const auto backend = _backend.lock();
        if (!backend)
        {
            TBX_TRACE_ERROR_ONCE("Toybox renderer graphics backend is unavailable.");
            return;
        }

        const auto vsync_mode = settings.vsync_enabled;
        if (backend->get_vsync() != vsync_mode)
        {
            const auto vsync_result = backend->set_vsync(vsync_mode);
            if (!vsync_result)
            {
                TBX_TRACE_ERROR_ONCE(
                    "Toybox renderer failed to apply vsync setting. {}",
                    vsync_result.get_report());
            }
        }

        const auto result = _pipeline.execute(settings, delta_time);
        if (!result)
        {
            TBX_TRACE_ERROR("Toybox rendering pipeline execution failed. {}", result.get_report());
        }
    }

    void Rendering::wait_for_render_frame() noexcept
    {
        if (!_render_future.valid())
            return;

        TBX_TRY_CATCH_ASSERT(_render_future.get();, "Toybox renderer frame completion failed.");
    }
}
