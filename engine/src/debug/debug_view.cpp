#include "tbx/debug/debug_view.h"
#include "tbx/app.h"
#include "tbx/core/log.h"
#include "tbx/files/files.h"
#include "tbx/ui/ui.h"
#include <filesystem>
#include <format>

namespace tbx::debug
{
    /// @brief
    /// Purpose: Overlay bookkeeping: its document, visibility, and smoothed timings.
    struct DebugState
    {
        uint64 document = 0;
        bool is_open = false;
        float smoothed_delta = 0.0f;
        float refresh_timer = 0.0f;
    };

    static DebugState g_debug = {};

    bool is_open()
    {
        return g_debug.is_open;
    }

    void reset()
    {
        g_debug = {}; // the document itself dies with ui::reset()
    }

    void set_open(const bool is_open)
    {
        g_debug.is_open = is_open;
        if (is_open && g_debug.document == 0)
        {
            const auto path = std::filesystem::path(TBX_RESOURCES_PATH) / "Ui" / "debug.rml";
            const auto text = files::read_text(path);
            if (!text)
            {
                log_error("debug view: {}", text.error());
                return;
            }
            if (const auto loaded = ui::load_document(*text))
                g_debug.document = *loaded;
            else
                log_error("debug view: {}", loaded.error());
        }
        if (g_debug.document != 0)
            ui::set_document_visible(g_debug.document, is_open);
    }

    void toggle()
    {
        set_open(!g_debug.is_open);
    }

    void update(const App& app)
    {
        if (!g_debug.is_open || g_debug.document == 0)
            return;
        g_debug.smoothed_delta = g_debug.smoothed_delta <= 0.0f
            ? app.delta_time
            : g_debug.smoothed_delta * 0.9f + app.delta_time * 0.1f;
        g_debug.refresh_timer -= app.delta_time;
        if (g_debug.refresh_timer > 0.0f)
            return;
        g_debug.refresh_timer = 0.25f;

        const float fps =
            g_debug.smoothed_delta > 0.0f ? 1.0f / g_debug.smoothed_delta : 0.0f;
        ui::set_text(
            "debug_fps",
            std::format("{:.0f} fps  ({:.2f} ms)", fps, g_debug.smoothed_delta * 1000.0f));
        ui::set_text("debug_frame", std::format("frame {}", app.frame));
        ui::set_text("debug_toys", std::format("toys: {}", get_sandbox().get_toy_count()));
        ui::set_text(
            "debug_assets",
            std::format("assets resident: {}", get_assets().get_loaded_count()));
        ui::set_text(
            "debug_viewport",
            std::format(
                "viewport: {}x{}",
                get_window().get_width(),
                get_window().get_height()));
    }
}
