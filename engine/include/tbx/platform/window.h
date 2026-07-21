#pragma once
#include "tbx/utils/api.h"
#include "tbx/events/events.h"
#include <memory>
#include <cstddef>
#include <span>
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Window creation parameters; headless skips the OS window and GL context
    /// entirely (tests/tooling).
    struct TBX_API WindowDescription
    {
        std::string title = "Toybox";
        int width = 1600;
        int height = 900;
        bool is_headless = false;
    };

    /// @brief
    /// Purpose: The concrete platform boundary (see cmake/tbx_backend.cmake): the selected
    /// backend folder (platform/sdl/) provides the implementation, and its library types never
    /// escape it.
    /// @details
    /// Ownership: Owns the OS window and GL context via RAII. Thread Safety: Main thread only.
    class TBX_API Window final
    {
      public:
        explicit Window(const WindowDescription& description);
        ~Window();

      public:
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

      public:
        /// @brief
        /// Purpose: True when running without an OS window or GL context.
        bool is_headless() const;

        /// @brief
        /// Purpose: Framebuffer height in pixels.
        int get_height() const;

        /// @brief
        /// Purpose: Polls OS events into tbx::input and the given Events; returns false when
        /// the user closed the window. Called once per frame by Engine::pump().
        bool pump(Events& events);

        /// @brief
        /// Purpose: Sets the OS window title.
        void set_title(const std::string& title);

        /// @brief
        /// Purpose: Enables/disables vertical sync on the presented frame.
        void set_vsync(bool is_enabled);

        /// @brief
        /// Purpose: Sets the window/taskbar icon from RGBA8 pixels.
        void set_icon(int width, int height, std::span<const std::byte> rgba_pixels);

        /// @brief
        /// Purpose: Presents the current frame (no-op when headless).
        void swap();

        /// @brief
        /// Purpose: Framebuffer width in pixels.
        int get_width() const;

      private:
        struct State; // defined by the platform backend's .cpp

      private:
        std::unique_ptr<State> _state;
    };
}
