#include "tbx/debug/debug_view.h"
#include "tbx/platform/input.h"
#include "tbx/app.h"
#include "tbx/debug/log.h"
#include "tbx/ui/ui.h"
#include "tbx/ui/ui_document.h"
#include <filesystem>
#include <format>

namespace tbx::debug::view
{
    void update(
        DebugState& state,
        const input::InputState& input,
        const Sandbox& sandbox,
        const assets::AssetsState& assets,
        const windows::WindowsState& windows,
        ui::UiState& ui,
        const float delta_time)
    {
        ++state.frame;
        // The overlay owns its own hotkey.
        if (input::is_pressed(input, Key::F3))
            state.is_open = !state.is_open;
        if (!state.is_open)
            return;
        if (state.document.source.empty())
        {
            // First open: the overlay document is an engine-shipped file.
            const auto path = std::filesystem::path(TBX_RESOURCES_PATH) / "Ui" / "debug.rml";
            if (auto document = load<UiDocument>(path))
                state.document = std::move(*document);
            else
            {
                TBX_ERROR("debug view: {}", document.error());
                state.is_open = false;
                return;
            }
        }
        state.smoothed_delta = state.smoothed_delta <= 0.0f
                                   ? delta_time
                                   : state.smoothed_delta * 0.9f + delta_time * 0.1f;
        state.refresh_timer -= delta_time;
        if (state.refresh_timer > 0.0f)
            return;
        state.refresh_timer = 0.25f;

        const float fps = state.smoothed_delta > 0.0f ? 1.0f / state.smoothed_delta : 0.0f;
        ui.bindings["debug_fps"] = std::format("{:.0f} fps  ({:.2f} ms)", fps, state.smoothed_delta * 1000.0f);
        ui.bindings["debug_frame"] = std::format("frame {}", state.frame);
        ui.bindings["debug_toys"] = std::format("toys: {}", sandbox.get_toy_count());
        ui.bindings["debug_assets"] = std::format("assets resident: {}", assets::get_loaded_count(assets));
        const int width = windows.windows.empty() ? 0 : windows.windows.front().width;
        const int height = windows.windows.empty() ? 0 : windows.windows.front().height;
        ui.bindings["debug_viewport"] = std::format("viewport: {}x{}", width, height);
    }
}
