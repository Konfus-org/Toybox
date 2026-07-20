#pragma once
#include "tbx/events/events.h"
#include "tbx/files/files.h"
#include "tbx/jobs/jobs.h"
#include "tbx/platform/input.h"
#include "tbx/platform/window.h"
#include <filesystem>
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Startup parameters for the engine; headless runs without a window or GPU
    /// (tests/tooling).
    struct EngineConfig
    {
        std::string title = "Toybox";
        int width = 1600;
        int height = 900;
        bool headless = false;
        std::filesystem::path asset_root = {};
    };

    /// @brief
    /// Purpose: The composition root. Subsystems are public members constructed in declaration
    /// order — that order IS the dependency graph, and reverse-order destruction IS shutdown.
    /// @details
    /// Ownership: Owns every subsystem by value; there are no getters and no service locator.
    /// The engine does not own main() or the loop: hosts (tbx_run, a game exe, the future
    /// editor) drive pump/update/render. Thread Safety: Main thread only.
    class Engine final
    {
      public:
        explicit Engine(const EngineConfig& config);
        ~Engine();

      public:
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

      public:
        /// @brief
        /// Purpose: Sets up the frame's viewport and clear; hosts draw between begin_frame()
        /// and render(). (Grows into RenderView/sandbox rendering with M6.)
        void begin_frame();

        /// @brief
        /// Purpose: OS events → Input/window, then main-thread jobs, then queued events.
        /// Returns false when the user closed the window.
        bool pump();

        /// @brief
        /// Purpose: Presents the frame.
        void render();

        /// @brief
        /// Purpose: Variable-step frame update; runs fixed-cadence work at FIXED_STEP inside.
        void update(float dt);

      public:
        Jobs jobs;
        Files files;
        Events events;
        Window window;
        Input input;
        // Later milestones add: Assets assets; Renderer renderer; Physics physics;
        // Sandbox sandbox; Scripts scripts;

      private:
        static constexpr float FIXED_STEP = 1.0f / 60.0f;

      private:
        float _fixed_accumulator = 0.0f;
    };
}
