#pragma once
#include "tbx/api.h"
#include "tbx/events/events.h"
#include "tbx/platform/input.h"
#include "tbx/utils/typedefs.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: A window's lifecycle: OPEN until the user closes it (update_windows() observes the OS
    /// close request; CLOSED windows hide and stay closed).
    enum class WindowStatus : uint8
    {
        OPEN = 0,
        CLOSED
    };

    /// @brief
    /// Purpose: One window as plain data: write the fields, the next update_windows() applies them to
    /// the OS window (created lazily by the first update_windows()). width/height are the creation
    /// size until then and the actual pixel size after — the backend writes resizes back.
    /// Cameras aim at a window by its name. The OS handles live behind the Backend seam
    /// (platform/sdl/); its library types never escape that folder.
    struct TBX_DLL_EXPORT Window
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
    struct TBX_DLL_EXPORT WindowsState
    {
        std::vector<Window> open_windows = {};
    };

    /// @brief
    /// Purpose: Runs the windows for one frame: presents what was drawn since the last call
    /// (each window's first frame skips cleanly), materializes OS windows for new entries
    /// (the first one brings up the shared GL context), applies changed data (title, icon,
    /// cursor mode, and the gpu module's vsync request), and pumps OS events into the input
    /// state and event signals. Called by tbx::run() every frame.
    TBX_DLL_EXPORT void update_windows(WindowsState& state, InputState& input, EventsState& events);

    /// @brief
    /// Purpose: Binds the shared GL context to this window's surface — subsequent gpu calls
    /// draw into it. Called per window by the render loop; no-op before the first update_windows().
    TBX_DLL_EXPORT void make_current(const Window& window);
}
