#include "tbx/debug/debug_view.h"
#include "tbx/app.h"
#include "tbx/debug/log.h"
#include "tbx/ui/ui.h"
#include "tbx/ui/ui_document.h"
#include <filesystem>
#include <format>

namespace tbx::debug::view
{
    /// @brief
    /// Purpose: Overlay bookkeeping: its document text, visibility, and smoothed timings.
    struct DebugState
    {
        UiDocument document = {};
        bool is_open = false;
        float smoothed_delta = 0.0f;
        float refresh_timer = 0.0f;
    };

    static DebugState g_debug = {};

    std::optional<std::reference_wrapper<const UiDocument>> get_document()
    {
        if (!g_debug.is_open || g_debug.document.text.empty())
            return {};
        return std::cref(g_debug.document);
    }

    bool is_open()
    {
        return g_debug.is_open;
    }

    void reset()
    {
        g_debug = {};
    }

    void set_open(const bool is_open)
    {
        g_debug.is_open = is_open;
        if (is_open && g_debug.document.text.empty())
        {
            const auto path = std::filesystem::path(TBX_RESOURCES_PATH) / "Ui" / "debug.rml";
            if (auto document = load<UiDocument>(path))
                g_debug.document = std::move(*document);
            else
                TBX_ERROR("debug view: {}", document.error());
        }
    }

    void toggle()
    {
        set_open(!g_debug.is_open);
    }

    void update(const App& app)
    {
        if (!g_debug.is_open || g_debug.document.text.empty())
            return;
        g_debug.smoothed_delta = g_debug.smoothed_delta <= 0.0f
            ? app.state.delta_time
            : g_debug.smoothed_delta * 0.9f + app.state.delta_time * 0.1f;
        g_debug.refresh_timer -= app.state.delta_time;
        if (g_debug.refresh_timer > 0.0f)
            return;
        g_debug.refresh_timer = 0.25f;

        const float fps =
            g_debug.smoothed_delta > 0.0f ? 1.0f / g_debug.smoothed_delta : 0.0f;
        ui::set_string(
            "debug_fps",
            std::format("{:.0f} fps  ({:.2f} ms)", fps, g_debug.smoothed_delta * 1000.0f));
        ui::set_string("debug_frame", std::format("frame {}", app.state.frame));
        ui::set_string("debug_toys", std::format("toys: {}", get_sandbox().get_toy_count()));
        ui::set_string(
            "debug_assets",
            std::format("assets resident: {}", assets::get_loaded_count()));
        ui::set_string(
            "debug_viewport",
            std::format(
                "viewport: {}x{}",
                get_window().get_width(),
                get_window().get_height()));
    }
}
