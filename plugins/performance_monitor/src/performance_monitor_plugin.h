#pragma once
#include "debug_window_title_monitor.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/app/application.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include "tbx/systems/time/delta_time.h"

namespace tbx::performance_monitor
{
    struct FpsInfo
    {
        double average_fps = 0.0;
        double average_frame_time_ms = 0.0;
    };

    /// @brief
    /// Purpose: Collects frame profiling data and reports runtime debug/performance diagnostics.
    /// @details
    /// Ownership: Does not own application services; samples frame timing from app lifecycle
    /// events. Thread Safety: Not thread-safe; expected to run on the main thread.
    [[tbx::plugin(
        name = "PerformanceMonitor",
        version = "1.0.0",
        category = tbx::PluginCategory::LOGGING)]];
    class TBX_PLUGIN_API PerformanceMonitor final : public tbx::Plugin
    {
      public:
        PerformanceMonitor() = default;
        ~PerformanceMonitor() noexcept override = default;

      public:
        PerformanceMonitor(const PerformanceMonitor&) = delete;
        PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;
        PerformanceMonitor(PerformanceMonitor&&) noexcept = default;
        PerformanceMonitor& operator=(PerformanceMonitor&&) noexcept = default;

      public:
        void on_attach() override;
        void on_detach() override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void initialize_main_window(tbx::Application& application);
        void record_frame(const tbx::DeltaTime& dt);
        void trace_perf_info();
        void reset_performance_sample();
        FpsInfo calculate_fps_averages();

#if !defined(TBX_FULL_RELEASE)
        void update_debug_main_window_title(tbx::Application& application);
#endif

      private:
        tbx::Window _main_window = {};
        std::string _main_window_base_title = {};

        double _performance_sample_elapsed_seconds = 0.0;
        uint _performance_sample_frame_count = 0U;
        double _performance_sample_min_frame_time_ms = 0.0;
        double _performance_sample_max_frame_time_ms = 0.0;
        bool _performance_sample_has_data = false;

#if !defined(TBX_FULL_RELEASE)
        std::string _debug_main_window_title = {};
        DebugWindowTitleMonitor _debug_window_title_monitor = {};
#endif
    };
}
