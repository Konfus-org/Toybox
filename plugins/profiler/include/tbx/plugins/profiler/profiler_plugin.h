#pragma once
#include "tbx/interfaces/plugin.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/typedefs.h"
#include <functional>
#include <optional>
#include <string>

namespace tbx
{
    class Application;
}

namespace profiler
{
    /// @brief
    /// Purpose: Collects frame profiling data and reports runtime debug/performance diagnostics.
    /// @details
    /// Ownership: Does not own application services; samples frame timing from app lifecycle
    /// events. Thread Safety: Not thread-safe; expected to run on the main thread.
    class TBX_PLUGIN_API ProfilerPlugin final : public tbx::Plugin
    {
      public:
        ProfilerPlugin() = default;
        ~ProfilerPlugin() noexcept override = default;

      public:
        ProfilerPlugin(const ProfilerPlugin&) = delete;
        ProfilerPlugin& operator=(const ProfilerPlugin&) = delete;
        ProfilerPlugin(ProfilerPlugin&&) noexcept = default;
        ProfilerPlugin& operator=(ProfilerPlugin&&) noexcept = default;

      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void initialize_main_window(tbx::Application& application);
        void record_frame(const tbx::DeltaTime& dt);
        void reset_performance_sample();

#if defined(TBX_DEBUG)
        void update_debug_main_window_title(const tbx::DeltaTime& dt);
#endif

      private:
        std::optional<std::reference_wrapper<tbx::ServiceProvider>> _service_provider =
            std::nullopt;
        tbx::Window _main_window = {};
        std::string _main_window_base_title = {};

        double _performance_sample_elapsed_seconds = 0.0;
        uint _performance_sample_frame_count = 0U;
        double _performance_sample_min_frame_time_ms = 0.0;
        double _performance_sample_max_frame_time_ms = 0.0;
        bool _performance_sample_has_data = false;

#if defined(TBX_DEBUG)
        std::string _debug_main_window_title = {};
        double _debug_window_title_elapsed_seconds = 0.0;
        uint _debug_window_title_frame_count = 0U;
#endif
    };
}
