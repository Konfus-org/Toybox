#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/typedefs.h"
#include <memory>
#include <string>

namespace tbx
{
    class Application;
    class AppSettings;
}

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
    class TBX_PLUGIN_API TbxPerformanceMonitorPlugin final : public tbx::Plugin
    {
      public:
        TbxPerformanceMonitorPlugin() = default;
        ~TbxPerformanceMonitorPlugin() noexcept override = default;

      public:
        TbxPerformanceMonitorPlugin(const TbxPerformanceMonitorPlugin&) = delete;
        TbxPerformanceMonitorPlugin& operator=(const TbxPerformanceMonitorPlugin&) = delete;
        TbxPerformanceMonitorPlugin(TbxPerformanceMonitorPlugin&&) noexcept = default;
        TbxPerformanceMonitorPlugin& operator=(TbxPerformanceMonitorPlugin&&) noexcept = default;

      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach(tbx::ServiceProvider& service_provider) override;
        void on_update(const tbx::DeltaTime& dt) override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void initialize_main_window(tbx::Application& application);
        void record_frame(const tbx::DeltaTime& dt);
        void trace_perf_info();
        void reset_performance_sample();
        FpsInfo calculate_fps_averages();

#if !defined(TBX_FULL_RELEASE)
        void update_debug_main_window_title(const tbx::DeltaTime& dt);
#endif

      private:
        tbx::ServiceProvider* _service_provider = nullptr;
        std::weak_ptr<tbx::IWindowManager> _window_manager = {};
        std::weak_ptr<tbx::AppSettings> _settings = {};
        tbx::Window _main_window = {};
        std::string _main_window_base_title = {};

        double _performance_sample_elapsed_seconds = 0.0;
        uint _performance_sample_frame_count = 0U;
        double _performance_sample_min_frame_time_ms = 0.0;
        double _performance_sample_max_frame_time_ms = 0.0;
        bool _performance_sample_has_data = false;

#if !defined(TBX_FULL_RELEASE)
        std::string _debug_main_window_title = {};
        double _debug_window_title_elapsed_seconds = 0.0;
        uint _debug_window_title_frame_count = 0U;
#endif
    };
}
