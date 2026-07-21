#pragma once
#include "tbx/utils/api.h"
#include "tbx/assets/assets.h"
#include "tbx/audio/audio_listener.h"
#include "tbx/audio/audio_source.h"
#include "tbx/gfx/camera.h"
#include "tbx/gfx/directional_light.h"
#include "tbx/gfx/post_processing.h"
#include "tbx/gfx/renderer.h"
#include "tbx/gfx/sky.h"
#include "tbx/math/transform.h"
#include "tbx/physics/collider.h"
#include "tbx/physics/rigid_body.h"
#include "tbx/scripting/script.h"
#include "tbx/ui/ui_block.h"
#include "tbx/utils/typedefs.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include "tbx/gfx/render_graph.h"
#include "tbx/jobs/jobs.h"
#include "tbx/platform/window.h"
#include "tbx/scripting/scripts.h"
#include <filesystem>
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Graphics configuration applied at boot.
    struct TBX_API GraphicsSettings
    {
        bool is_vsync_enabled = true;
        int shadow_resolution = 2048;
    };

    /// @brief
    /// Purpose: Physics configuration applied at boot; fixed_timestep also paces
    /// fixed_update scripting.
    struct TBX_API PhysicsSettings
    {
        float fixed_timestep = 1.0f / 60.0f;
        Vec3 gravity = Vec3(0.0f, -9.81f, 0.0f);
    };

    /// @brief
    /// Purpose: Audio configuration applied at boot.
    struct TBX_API AudioSettings
    {
        float master_volume = 1.0f;
    };

    /// @brief
    /// Purpose: Asset system configuration applied at boot.
    struct TBX_API AssetSettings
    {
        float idle_lifetime_seconds = 60.0f;
    };

    /// @brief
    /// Purpose: The application, as pure data: configuration in, per-frame data out. The whole
    /// runtime is one loop — `while (tbx::run(app)) { gpu::begin_frame(); ... }` — and run()
    /// fills the frame fields each iteration. Author it as a .tapp file and load_app() it, or
    /// fill it in code; the reflected fields are exactly the .tapp schema.
    struct TBX_API App
    {
        // Configuration (read once, at the first run() call).
        std::string title = "Toybox";
        int width = 1600;
        int height = 900;
        bool is_headless = false;
        std::filesystem::path asset_root = {};
        AssetHandle<Box> sandbox = {};   // the .box (box of kits) the boot opens
        AssetHandle<Texture> icon = {};  // the window/taskbar icon
        AssetHandle<Json> config = {};   // the .tapp itself (set by load_app; watched live)
        GraphicsSettings graphics = {};
        PhysicsSettings physics = {};
        AudioSettings audio = {};
        AssetSettings assets = {};

        // Per-frame data (written by run()).
        float delta_time = 0.0f;
        uint64 frame = 0;
        bool is_running = false;
    };

    /// @brief
    /// Purpose: Loads a .tapp application config: the file deserializes straight into the App
    /// struct (missing keys keep their defaults; sandbox/icon accept asset paths or uuids)
    /// and the asset root becomes the .tapp's folder.
    TBX_API Result<App> load_app(const std::filesystem::path& tapp_file);

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
    /// Purpose: Registers every builtin block (Transform, Camera, Renderer,
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
    /// Purpose: The standard renderer (default pass list; hosts may reshape it).
    TBX_API RenderGraph& get_render_graph();

    /// @brief
    /// Purpose: The scripting system.
    TBX_API Scripts& get_scripts();

    /// @brief
    /// Purpose: The OS window.
    TBX_API Window& get_window();
}
