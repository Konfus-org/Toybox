#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/debugging/macros.h"

namespace tbx
{
    constexpr auto RENDER_LANE_NAME = std::string_view("render");

    Rendering::Rendering(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<ThreadManager> thread_manager,
        std::weak_ptr<IWindowManager> window_manager,
        const GraphicsSettings& settings)
        : _thread_manager(std::move(thread_manager))
        , _backend(std::move(backend))
        , _window_manager(window_manager)
        , _settings(settings)
        , _pipeline(_backend, std::move(asset_manager), std::move(window_manager))
    {
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

    void Rendering::render(const DeltaTime& delta_time)
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

    void Rendering::receive_message(Message& msg)
    {
        const auto graphics_settings_event = handle_property_changed<&AppSettings::graphics>(msg);
        if (!graphics_settings_event)
            return;

        auto updated_settings = graphics_settings_event->get().current;
        {
            std::lock_guard lock(_settings_mutex);
            _settings = updated_settings;
        }
    }

    void Rendering::render_frame(const DeltaTime& delta_time)
    {
        const auto backend = _backend.lock();
        if (!backend)
        {
            TBX_TRACE_ERROR_ONCE("Toybox renderer graphics backend is unavailable.");
            return;
        }

        const auto settings = [this]()
        {
            std::lock_guard lock(_settings_mutex);
            return _settings;
        }();

        const auto vsync_mode = settings.vsync_enabled.value;
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

        const auto result = _pipeline.execute(*backend, settings, delta_time);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE(
                "Toybox rendering pipeline execution failed. {}",
                result.get_report());
        }
    }

    void Rendering::wait_for_render_frame() noexcept
    {
        if (!_render_future.valid())
            return;

        TBX_TRY_CATCH_ASSERT(_render_future.get();, "Toybox renderer frame completion failed.");
    }
}
