#include "debug_window_title_monitor.h"
#include <format>

namespace tbx::performance_monitor
{
    constexpr double DEBUG_TITLE_UPDATE_INTERVAL_SECONDS = 0.25;
    constexpr double DEBUG_TITLE_WARMUP_SECONDS = 1.0;
    constexpr uint DEBUG_TITLE_WARMUP_FRAMES = 30U;

    static std::string make_placeholder_title(
        std::string_view base_title,
        const tbx::GraphicsApi api)
    {
        return std::format("{} [{}, FPS: ---, Frame: --- ms]", base_title, api);
    }

    static std::string make_sampled_title(
        std::string_view base_title,
        const tbx::GraphicsApi api,
        const double sample_elapsed_seconds,
        const uint sample_frame_count)
    {
        if (sample_elapsed_seconds <= 0.0 || sample_frame_count == 0U)
            return make_placeholder_title(base_title, api);

        const auto average_fps =
            static_cast<double>(sample_frame_count) / sample_elapsed_seconds;
        const auto average_frame_time_ms =
            (sample_elapsed_seconds * 1000.0) / static_cast<double>(sample_frame_count);
        return std::format(
            "{} [{}, FPS: {}, Frame: {:.2f} ms]",
            base_title,
            api,
            static_cast<int>(average_fps),
            average_frame_time_ms);
    }

    void DebugWindowTitleMonitor::record_frame(const tbx::DeltaTime& dt)
    {
        _title_update_elapsed_seconds += dt.seconds;

        if (!_has_completed_warmup)
        {
            _warmup_elapsed_seconds += dt.seconds;
            ++_warmup_frame_count;
            if (_warmup_elapsed_seconds < DEBUG_TITLE_WARMUP_SECONDS
                || _warmup_frame_count < DEBUG_TITLE_WARMUP_FRAMES)
            {
                return;
            }

            _has_completed_warmup = true;
            _live_sample_elapsed_seconds = 0.0;
            _live_sample_frame_count = 0U;
        }

        _live_sample_elapsed_seconds += dt.seconds;
        ++_live_sample_frame_count;
    }

    void DebugWindowTitleMonitor::reset()
    {
        _title_update_elapsed_seconds = 0.0;
        _warmup_elapsed_seconds = 0.0;
        _live_sample_elapsed_seconds = 0.0;
        _warmup_frame_count = 0U;
        _live_sample_frame_count = 0U;
        _has_completed_warmup = false;
    }

    std::optional<std::string> DebugWindowTitleMonitor::consume_title(
        std::string_view base_title,
        const tbx::GraphicsApi api)
    {
        if (_title_update_elapsed_seconds < DEBUG_TITLE_UPDATE_INTERVAL_SECONDS)
            return std::nullopt;

        _title_update_elapsed_seconds = 0.0;

        if (!_has_completed_warmup)
            return make_placeholder_title(base_title, api);

        const auto title = make_sampled_title(
            base_title,
            api,
            _live_sample_elapsed_seconds,
            _live_sample_frame_count);
        _live_sample_elapsed_seconds = 0.0;
        _live_sample_frame_count = 0U;
        return title;
    }
}
