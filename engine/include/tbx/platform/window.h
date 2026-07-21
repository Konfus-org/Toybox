#pragma once
#include "tbx/core/api.h"
#include "tbx/events/events.h"
#include <memory>
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
