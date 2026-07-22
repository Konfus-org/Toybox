#pragma once
#include "tbx/events/events.h"
#include "tbx/platform/input.h"
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace tbx::windows
{
    /// @brief
    /// Purpose: A window's lifecycle: OPEN until the user closes it (update() observes the OS
    /// close request; CLOSED windows hide and stay closed).
    enum class WindowStatus : uint8
    {
        OPEN = 0,
        CLOSED
    };

    /// @brief
    /// Purpose: One window as plain data: write the fields, the next update() applies them to
    /// the OS window (created lazily by the first update()). width/height are the creation
    /// size until then and the actual pixel size after — the backend writes resizes back.
    /// Cameras aim at a window by its name. The OS handles live behind the Backend seam
    /// (platform/sdl/); its library types never escape that folder.
    struct TBX_API Window
    {
        struct Backend; // defined by the platform backend's .cpp

        Window();
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) noexcept;
        Window& operator=(Window&&) noexcept;

        std::string name = "main";
        std::string title = "Toybox";
        int width = 1600;
        int height = 900;
        bool is_vsync_enabled = false;
        WindowStatus status = WindowStatus::OPEN;
        int icon_width = 0;
        int icon_height = 0;
        std::vector<std::byte> icon_pixels = {};
        std::unique_ptr<Backend> backend = {};
    };

    /// @brief
    /// Purpose: Every window the runtime owns, held by value on the Runtime. Empty =
    /// headless (tests/tooling). The first window is the main one: it carries the app's
    /// config, the UI, the cursor mode, and closing it stops the app; closing any other
    /// window just closes that window.
    struct TBX_API WindowsState
    {
        std::vector<Window> windows = {};
    };

    /// @brief
    /// Purpose: Runs the windows for one frame: presents what was drawn since the last call
    /// (each window's first frame skips cleanly), materializes OS windows for new entries
    /// (the first one brings up the shared GL context), applies changed data (title, vsync,
    /// icon, cursor mode), and pumps OS events into the input state and event signals.
    /// Called by tbx::run() every frame.
    TBX_API void update(
        WindowsState& state,
        input::InputState& input,
        events::EventsState& events);

    /// @brief
    /// Purpose: Binds the shared GL context to this window's surface — subsequent gpu calls
    /// draw into it. Called per window by the render loop; no-op before the first update().
    TBX_API void make_current(const Window& window);
}
