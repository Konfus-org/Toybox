#include "performance_monitor_plugin.h"
#include "tbx/systems/app/application.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/debugging/macros.h"

namespace tbx::performance_monitor
{

    void PerformanceMonitor::on_attach()
    {
        reset_performance_sample();
#if !defined(TBX_FULL_RELEASE)
        _debug_window_title_monitor.reset();
#endif
    }

    void PerformanceMonitor::on_detach()
    {
        _main_window = {};
        _main_window_base_title.clear();

        trace_perf_info();
        reset_performance_sample();

#if !defined(TBX_FULL_RELEASE)
        _debug_main_window_title.clear();
        _debug_window_title_monitor.reset();
#endif
    }

    void PerformanceMonitor::on_recieve_message(tbx::Message& msg)
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
#if !defined(TBX_FULL_RELEASE)
            _debug_window_title_monitor.record_frame(dt);
            update_debug_main_window_title(update_end_event->get().application);
#endif
        }
    }

    void PerformanceMonitor::initialize_main_window(tbx::Application& application)
    {
        _main_window = application.get_main_window();
        _main_window_base_title = application.get_name().empty() ? std::string("Toybox Application")
                                                                 : application.get_name();

        if (!_main_window.id.is_valid())
            return;

        auto window_manager =
            application.get_service_provider().try_get_service<tbx::IWindowManager>().lock();
        if (window_manager && window_manager->has(_main_window))
            _main_window_base_title = window_manager->get_title(_main_window);
    }

    void PerformanceMonitor::record_frame(const tbx::DeltaTime& dt)
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

        trace_perf_info();
        reset_performance_sample();
    }

    void PerformanceMonitor::reset_performance_sample()
    {
        _performance_sample_elapsed_seconds = 0.0;
        _performance_sample_frame_count = 0U;
        _performance_sample_min_frame_time_ms = 0.0;
        _performance_sample_max_frame_time_ms = 0.0;
        _performance_sample_has_data = false;
    }

    void PerformanceMonitor::trace_perf_info()
    {
        auto fps_info = calculate_fps_averages();
        TBX_TRACE_INFO(
            "FPS(avg): {:.2f}, Frame Time(avg): {:.2f}ms, Frame Time(min/max): {:.2f}/{:.2f}ms",
            fps_info.average_fps,
            fps_info.average_frame_time_ms,
            _performance_sample_min_frame_time_ms,
            _performance_sample_max_frame_time_ms);
        if (fps_info.average_fps < 30.0)
        {
            TBX_TRACE_WARNING(
                "Average FPS is below 30! Consider optimizing your application or "
                "investigating potential performance issues.");
        }
    }

#if !defined(TBX_FULL_RELEASE)
    void PerformanceMonitor::update_debug_main_window_title(tbx::Application& application)
    {
        if (!_main_window.id.is_valid())
            return;

        auto window_manager =
            application.get_service_provider().try_get_service<tbx::IWindowManager>().lock();
        if (!window_manager || !window_manager->is_open(_main_window))
            return;

        const auto& settings = application.get_settings();
        const auto next_title = _debug_window_title_monitor.consume_title(
            _main_window_base_title,
            settings.graphics.graphics_api);
        if (!next_title.has_value())
            return;

        if (_debug_main_window_title != *next_title)
        {
            window_manager->set_title(_main_window, *next_title);
            _debug_main_window_title = *next_title;
        }
    }
#endif

    FpsInfo PerformanceMonitor::calculate_fps_averages()
    {
        double average_fps = 0.0;
        double average_frame_time_ms = 0.0;

        if (_performance_sample_elapsed_seconds > 0.0 && _performance_sample_frame_count > 0U)
        {
            average_fps = static_cast<double>(_performance_sample_frame_count)
                          / _performance_sample_elapsed_seconds;
            average_frame_time_ms = (_performance_sample_elapsed_seconds * 1000.0)
                                    / static_cast<double>(_performance_sample_frame_count);
        }

        return {average_fps, average_frame_time_ms};
    }
}
