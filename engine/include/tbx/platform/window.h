#pragma once
#include "tbx/api.h"
#include "tbx/assets/assets.h"
#include "tbx/events/events.h"
#include "tbx/gfx/texture.h"
#include "tbx/platform/input.h"
#include "tbx/utils/typedefs.h"
#include <functional>
#include <memory>
#include <optional>
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
    /// Purpose: How a window fills the screen — an ordinary bordered window, a borderless one, or
    /// exclusive fullscreen. Write it on the Window (or through WindowHandle::set_mode); the next
    /// update_windows() applies it (as creation flags before the OS window exists, live after).
    enum class WindowMode : uint8
    {
        WINDOWED = 0,
        BORDERLESS,
        FULLSCREEN
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
        WindowMode mode = WindowMode::WINDOWED;
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
    /// Purpose: A fluent handle to one of the running runtime's windows — the way to open and
    /// configure a window: open(Window{}).set_mode(...).set_title(...).set_size(w, h). Keyed by a
    /// session-stable index into the windows state (the list only ever grows; a closed window keeps
    /// its slot), re-resolved on every call, so the handle survives the list reallocating and a
    /// set_title() renaming the very window it points at. Setters write plain data that the next
    /// update_windows() applies; read accessors return the current value (defaults when the handle
    /// no longer resolves). Main-thread only (reads tbx::internal::get_runtime()).
    class TBX_DLL_EXPORT WindowHandle final
    {
      public:
        WindowHandle() = default;
        explicit WindowHandle(size index);

        WindowHandle& set_mode(WindowMode mode);
        WindowHandle& set_title(std::string title);
        WindowHandle& set_size(uint32 width, uint32 height);
        WindowHandle& set_icon(AssetHandle<Texture> icon);

        bool is_valid() const;
        WindowMode get_mode() const;
        const std::string& get_title() const;
        uint32 get_width() const;
        uint32 get_height() const;

      private:
        /// @brief
        /// Purpose: The window this handle points at, or nothing when no runtime is published or the
        /// index is out of range.
        std::optional<std::reference_wrapper<Window>> resolve() const;

        size _index = 0;
    };

    /// @brief
    /// Purpose: Opens a window on the running runtime and returns a fluent handle to it — appends the
    /// window as plain data (its OS window materializes on the next update_windows(), so a fluent
    /// chain configures it before it is ever shown). Main-thread only.
    TBX_DLL_EXPORT WindowHandle open(Window window);

    /// @brief
    /// Purpose: The windows the running runtime owns, as plain data — the public query escape hatch
    /// over the (internal) windows state. Empty when headless. Read-only; write window data through
    /// the dedicated APIs. Main-thread only (reads tbx::internal::get_runtime()).
    TBX_DLL_EXPORT const std::vector<Window>& get_open_windows();

}
