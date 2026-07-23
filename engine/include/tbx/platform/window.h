#pragma once
#include "tbx/api.h"
#include "tbx/assets/assets.h"
#include "tbx/events/events.h"
#include "tbx/gfx/texture.h"
#include "tbx/platform/input.h"
#include "tbx/utils/typedefs.h"
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
    /// size until then and the actual pixel size after — the backend writes resizes back. The icon is
    /// an ordinary Texture asset, resolved (loaded, pixels read) by the backend when it changes.
    /// Cameras aim at a window by its title. The OS handles live behind the Backend seam
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

        std::string title = "Toybox";
        uint32 width = 1600;
        uint32 height = 900;
        AssetHandle<Texture> icon = {};
        WindowStatus status = WindowStatus::OPEN;
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
    /// Purpose: The windows the running runtime owns, as plain data — the public query escape hatch
    /// over the (internal) windows state. Empty when headless. Read-only; write window data through
    /// the dedicated APIs. Main-thread only (reads tbx::internal::current()).
    TBX_DLL_EXPORT const std::vector<Window>& get_open_windows();

    // ---- Internal (engine machinery; not the user-facing API) ----
    // The per-frame window pass and the low-level GL-context bind — driven by run()/the render loop.
    namespace internal
    {
        /// @brief
        /// Purpose: Runs the windows for one frame: presents what was drawn since the last call
        /// (each window's first frame skips cleanly), materializes OS windows for new entries
        /// (the first one brings up the shared GL context), applies changed data (title, icon —
        /// loaded from its Texture asset — cursor mode, and the gpu module's vsync request), and
        /// pumps OS events into the input state and event signals. Called by tbx::run() every frame.
        TBX_DLL_EXPORT void update_windows(
            WindowsState& state,
            InputState& input,
            EventsState& events,
            AssetsState& assets);

        /// @brief
        /// Purpose: Binds the shared GL context to this window's surface — subsequent gpu calls
        /// draw into it. Called per window by the render loop; no-op before the first update_windows().
        TBX_DLL_EXPORT void make_current(const Window& window);
    }
}
