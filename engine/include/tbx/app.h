#pragma once
#include "tbx/assets/assets.h"
#include "tbx/core/typedefs.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include "tbx/jobs/jobs.h"
#include "tbx/platform/window.h"
#include "tbx/scripting/scripts.h"
#include <filesystem>
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: The application, as pure data: configuration in, per-frame data out. The whole
    /// runtime is one loop — `while (tbx::run(app)) { gpu::begin_frame(); ... }` — and run()
    /// fills the frame fields each iteration.
    struct App
    {
        // Configuration (read once, at the first run() call).
        std::string title = "Toybox";
        int width = 1600;
        int height = 900;
        bool is_headless = false;
        std::filesystem::path asset_root = {};

        // Per-frame data (written by run()).
        float delta_time = 0.0f;
        uint64 frame = 0;
        bool is_running = false;
    };

    /// @brief
    /// Purpose: Runs one frame: presents the previous one, pumps OS events/jobs/events,
    /// updates streaming/scripts/fixed-step, and stamps the App's frame data. The first call
    /// boots the subsystems; returning false has already shut them down.
    bool run(App& app);

    /// @brief
    /// Purpose: Requests a clean exit — the next run() returns false.
    void quit();

    /// @brief
    /// Purpose: Registers every builtin block (Transform, Camera, MeshRenderer,
    /// DirectionalLight, RigidBody, Collider, Script, AudioListener, AudioSource) — THE one registration call.
    /// Idempotent; run() and every subsystem entry point call it, tests may too.
    void register_builtin_blocks();

    // The subsystems run() booted, for hosts and systems (RAII objects owned by the runtime;
    // valid between the first run() and the run() that returns false).

    /// @brief
    /// Purpose: The asset system.
    Assets& get_assets();

    /// @brief
    /// Purpose: The event signals.
    Events& get_events();

    /// @brief
    /// Purpose: The worker pool.
    Jobs& get_jobs();

    /// @brief
    /// Purpose: THE world container.
    Sandbox& get_sandbox();

    /// @brief
    /// Purpose: The scripting system.
    Scripts& get_scripts();

    /// @brief
    /// Purpose: The OS window.
    Window& get_window();
}
