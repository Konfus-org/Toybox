#pragma once
#include "tbx/core/api.h"
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
    struct TBX_API App
    {
        // Configuration (read once, at the first run() call).
        std::string title = "Toybox";
        int width = 1600;
        int height = 900;
        bool is_headless = false;
        std::filesystem::path asset_root = {};
        std::string sandbox = {}; // a .box layout (asset-relative) the boot opens
        std::string ui = {};      // a .rml document (asset-relative) the boot shows

        // Per-frame data (written by run()).
        float delta_time = 0.0f;
        uint64 frame = 0;
        bool is_running = false;
    };

    /// @brief
    /// Purpose: Runs one frame: presents the previous one, pumps OS events/jobs/events,
    /// updates streaming/scripts/fixed-step, and stamps the App's frame data. The first call
    /// boots the subsystems; returning false has already shut them down.
    TBX_API bool run(App& app);

    /// @brief
    /// Purpose: Requests a clean exit — the next run() returns false.
    TBX_API void quit();

    /// @brief
    /// Purpose: True between the first run() and shutdown — guards the get_* accessors for
    /// callers (script bindings, tools) that may exist without a running app.
    TBX_API bool is_app_running();

    /// @brief
    /// Purpose: Registers every builtin block (Transform, Camera, MeshRenderer,
    /// DirectionalLight, RigidBody, Collider, Script, AudioListener, AudioSource) — THE one registration call.
    /// Idempotent; run() and every subsystem entry point call it, tests may too.
    TBX_API void register_builtin_blocks();

    // The subsystems run() booted, for hosts and systems (RAII objects owned by the runtime;
    // valid between the first run() and the run() that returns false).

    /// @brief
    /// Purpose: The asset system.
    TBX_API Assets& get_assets();

    /// @brief
    /// Purpose: The event signals.
    TBX_API Events& get_events();

    /// @brief
    /// Purpose: The worker pool.
    TBX_API Jobs& get_jobs();

    /// @brief
    /// Purpose: THE world container.
    TBX_API Sandbox& get_sandbox();

    /// @brief
    /// Purpose: The scripting system.
    TBX_API Scripts& get_scripts();

    /// @brief
    /// Purpose: The document App::ui loaded at boot (0 when none) — pass it to
    /// ui::set_inline_style and friends.
    TBX_API uint64 get_ui_document();

    /// @brief
    /// Purpose: The OS window.
    TBX_API Window& get_window();
}
