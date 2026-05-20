#include "tbx/plugins/tbx_performance_monitor/tbx_performance_monitor_plugin.h"
#include "internal/tbx_performance_monitor_plugin_internal.h"
#include "tbx/systems/app/application.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/api.h"
#include <algorithm>
#include <cmath>

namespace tbx::performance_monitor
{

    void TbxPerformanceMonitorPlugin::on_attach(tbx::ServiceProvider& service_provider)
    {
        _service_provider = &service_provider;
        _window_manager = service_provider.try_get_service<tbx::IWindowManager>();
        _settings = service_provider.try_get_service<tbx::AppSettings>();
        reset_performance_sample();
    }

    void TbxPerformanceMonitorPlugin::on_detach(tbx::ServiceProvider&)
    {
        _service_provider = nullptr;
        _window_manager = {};
        _settings = {};
        _main_window = {};
        _main_window_base_title.clear();
        reset_performance_sample();

#if defined(TBX_DEBUG)
        _debug_main_window_title.clear();
        _debug_window_title_elapsed_seconds = 0.0;
        _debug_window_title_frame_count = 0U;
#endif
    }

    void TbxPerformanceMonitorPlugin::on_update(const tbx::DeltaTime& dt)
    {
#if defined(TBX_DEBUG)
        update_debug_main_window_title(dt);
#endif
    }

    void TbxPerformanceMonitorPlugin::on_recieve_message(tbx::Message& msg)
    {
        if (auto initialized_event = tbx::handle_message<tbx::ApplicationInitializedEvent>(msg))
        {
            initialize_main_window(initialized_event->get().application);
            return;
        }

        if (auto update_end_event = tbx::handle_message<tbx::ApplicationUpdateEndEvent>(msg))
        {
            const auto& dt = update_end_event->get().delta_time;
            record_frame(dt);
        }
    }

    void TbxPerformanceMonitorPlugin::initialize_main_window(tbx::Application& application)
    {
        if (!_service_provider)
            _service_provider = &application.get_service_provider();

        if (_window_manager.expired() && _service_provider)
            _window_manager = _service_provider->try_get_service<tbx::IWindowManager>();

        _main_window = application.get_main_window();
        _main_window_base_title = application.get_name().empty() ? std::string("Toybox Application")
                                                                 : application.get_name();

        if (!_main_window.is_valid())
            return;

        auto window_manager = _window_manager.lock();
        if (window_manager && window_manager->has(_main_window))
            _main_window_base_title = window_manager->get_title(_main_window);
    }

    void TbxPerformanceMonitorPlugin::record_frame(const tbx::DeltaTime& dt)
    {
        ++_performance_sample_frame_count;
        _performance_sample_elapsed_seconds += dt.seconds;

        if (!_performance_sample_has_data)
        {
            _performance_sample_min_frame_time_ms = dt.milliseconds;
            _performance_sample_max_frame_time_ms = dt.milliseconds;
            _performance_sample_has_data = true;
        }
        else
        {
            _performance_sample_min_frame_time_ms =
                std::min(_performance_sample_min_frame_time_ms, dt.milliseconds);
            _performance_sample_max_frame_time_ms =
                std::max(_performance_sample_max_frame_time_ms, dt.milliseconds);
        }

#if defined(TBX_DEBUG)
        constexpr double performance_log_interval_seconds = 10.0;
#else
        constexpr double performance_log_interval_seconds = 60.0;
#endif
        if (_performance_sample_elapsed_seconds < performance_log_interval_seconds)
            return;

        double average_fps = 0.0;
        double average_frame_time_ms = 0.0;

        if (_performance_sample_elapsed_seconds > 0.0 && _performance_sample_frame_count > 0U)
        {
            average_fps = static_cast<double>(_performance_sample_frame_count)
                          / _performance_sample_elapsed_seconds;
            average_frame_time_ms = (_performance_sample_elapsed_seconds * 1000.0)
                                    / static_cast<double>(_performance_sample_frame_count);
        }

        TBX_TRACE_INFO(
            "FPS(avg): {:.2f}, Frame Time(avg): {:.2f}ms, Frame Time(min/max): {:.2f}/{:.2f}ms",
            average_fps,
            average_frame_time_ms,
            _performance_sample_min_frame_time_ms,
            _performance_sample_max_frame_time_ms);

        reset_performance_sample();

        if (average_fps < 30.0)
        {
            TBX_TRACE_WARNING(
                "Average FPS is below 30! Consider optimizing your application or "
                "investigating potential performance issues.");
        }
    }

    void TbxPerformanceMonitorPlugin::reset_performance_sample()
    {
        _performance_sample_elapsed_seconds = 0.0;
        _performance_sample_frame_count = 0U;
        _performance_sample_min_frame_time_ms = 0.0;
        _performance_sample_max_frame_time_ms = 0.0;
        _performance_sample_has_data = false;
    }

#if defined(TBX_DEBUG)
    void TbxPerformanceMonitorPlugin::update_debug_main_window_title(const tbx::DeltaTime& dt)
    {
        if (!_main_window.is_valid())
            return;

        if (_window_manager.expired() && _service_provider)
            _window_manager = _service_provider->try_get_service<tbx::IWindowManager>();

        auto window_manager = _window_manager.lock();
        if (!window_manager || !window_manager->is_open(_main_window))
            return;

        _debug_window_title_elapsed_seconds += dt.seconds;
        ++_debug_window_title_frame_count;

        constexpr double debug_window_title_interval_seconds = 0.25;
        if (_debug_window_title_elapsed_seconds < debug_window_title_interval_seconds)
            return;

        auto average_fps = uint {0U};
        if (_debug_window_title_elapsed_seconds > 0.0 && _debug_window_title_frame_count > 0U)
        {
            const auto average_fps_value = static_cast<double>(_debug_window_title_frame_count)
                                           / _debug_window_title_elapsed_seconds;
            average_fps = static_cast<uint>(std::lround(average_fps_value));
        }

        auto settings = _settings.lock();
        if (!settings)
            return;

        const auto next_title = internal::build_debug_window_title(
            _main_window_base_title,
            settings->graphics.graphics_api,
            average_fps);

        if (_debug_main_window_title != next_title)
        {
            window_manager->set_title(_main_window, next_title);
            _debug_main_window_title = next_title;
        }

        _debug_window_title_elapsed_seconds = 0.0;
        _debug_window_title_frame_count = 0U;
    }
#endif
}
