#pragma once
#include "tbx/events/events.h"
#include "tbx/platform/input.h"
#include <memory>
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Window creation parameters; headless skips the OS window and GL context
    /// entirely (tests/tooling).
    struct WindowDesc
    {
        std::string title = "Toybox";
        int width = 1600;
        int height = 900;
        bool headless = false;
    };

    /// @brief
    /// Purpose: The concrete platform boundary (see cmake/tbx_backend.cmake): the selected
    /// backend folder (platform/sdl/) provides the implementation, and its library types never
    /// escape it.
    /// @details
    /// Ownership: Owns the OS window and GL context via RAII. Thread Safety: Main thread only.
    class Window final
    {
      public:
        explicit Window(const WindowDesc& desc);
        ~Window();

      public:
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

      public:
        /// @brief
        /// Purpose: GL function loader for the gfx backend (null when headless).
        using GlProc = void (*)();
        using GlProcLoader = GlProc (*)(const char* name);
        GlProcLoader gl_proc_loader() const;

        /// @brief
        /// Purpose: True when running without an OS window or GL context.
        bool headless() const;

        /// @brief
        /// Purpose: Framebuffer height in pixels.
        int height() const;

        /// @brief
        /// Purpose: Polls OS events into Input/Events; returns false when the user closed the
        /// window. Called once per frame by Engine::pump().
        bool pump(Input& input, Events& events);

        /// @brief
        /// Purpose: Presents the current frame (no-op when headless).
        void swap();

        /// @brief
        /// Purpose: Framebuffer width in pixels.
        int width() const;

      private:
        struct State; // defined by the platform backend's .cpp

      private:
        std::unique_ptr<State> _state;
    };
}
