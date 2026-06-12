#pragma once
#include "tbx/systems/graphics/api.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include "tbx/systems/time/delta_time.h"
#include <optional>
#include <string>
#include <string_view>

namespace tbx::performance_monitor
{
    /// @brief
    /// Purpose: Samples frame timing for the debug window title and hides metrics during warm-up.
    /// @details
    /// Ownership: Owns only transient sampling state for one monitor instance.
    /// Thread Safety: Not thread-safe; call from the main thread only.
    class TBX_PLUGIN_API DebugWindowTitleMonitor
    {
      public:
        DebugWindowTitleMonitor() = default;

      public:
        /// @brief
        /// Purpose: Records one frame of timing data for later title updates.
        /// @details
        /// Ownership: Copies the provided delta time by value into internal counters.
        /// Thread Safety: Not thread-safe; call from the main thread only.
        void record_frame(const tbx::DeltaTime& dt);

        /// @brief
        /// Purpose: Clears all warm-up, interval, and throttling state.
        /// @details
        /// Ownership: Resets this monitor instance in-place.
        /// Thread Safety: Not thread-safe; call from the main thread only.
        void reset();

        /// @brief
        /// Purpose: Builds the next debug title when the update interval has elapsed.
        /// @details
        /// Ownership: Returns an owned string when a title update is due.
        /// Thread Safety: Not thread-safe; call from the main thread only.
        std::optional<std::string> consume_title(
            std::string_view base_title,
            tbx::GraphicsApi api);

      private:
        double _title_update_elapsed_seconds = 0.0;
        double _warmup_elapsed_seconds = 0.0;
        double _live_sample_elapsed_seconds = 0.0;
        uint _warmup_frame_count = 0U;
        uint _live_sample_frame_count = 0U;
        bool _has_completed_warmup = false;
    };
}
