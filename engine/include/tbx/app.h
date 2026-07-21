#pragma once
#include "tbx/assets/assets.h"
#include "tbx/audio/audio_listener.h"
#include "tbx/audio/audio_source.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/events/events.h"
#include "tbx/gfx/camera.h"
#include "tbx/gfx/directional_light.h"
#include "tbx/gfx/post_processing.h"
#include "tbx/gfx/render_graph.h"
#include "tbx/gfx/renderer.h"
#include "tbx/gfx/sky.h"
#include "tbx/jobs/jobs.h"
#include "tbx/math/transform.h"
#include "tbx/physics/collider.h"
#include "tbx/physics/rigid_body.h"
#include "tbx/platform/window.h"
#include "tbx/scripting/script.h"
#include "tbx/scripting/scripts.h"
#include "tbx/ui/ui_block.h"
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
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
    /// Purpose: Per-frame data written by run() — runtime state, never serialized.
    struct TBX_API AppState
    {
        float delta_time = 0.0f;
        uint64 frame = 0;
        bool is_running = false;
    };

    /// @brief
    /// Purpose: What the app IS: window shape, content wiring, and where its assets live.
    struct TBX_API AppConfig
    {
        int width = 1600;
        int height = 900;
        bool is_headless = false;
        std::string title = "Toybox";
        AssetHandle<Box> sandbox = {};  // the .box the boot opens
        AssetHandle<Texture> icon = {}; // the window/taskbar icon

        // Derived by load_app(), never serialized: where assets live and which .tapp this
        // app came from (watched so changes re-apply live).
        std::filesystem::path asset_root = {};
        std::filesystem::path file = {};
    };

    /// @brief
    /// Purpose: Subsystem tuning applied at boot and re-applied live when the .tapp changes.
    struct TBX_API AppSettings
    {
        GraphicsSettings graphics = {};
        PhysicsSettings physics = {};
        AudioSettings audio = {};
        AssetSettings assets = {};
    };

    /// @brief
    /// Purpose: The application, as pure data: configuration in, per-frame data out. The
    /// whole runtime is one loop — `while (tbx::run(app)) { gpu::begin_frame(); ... }` — and
    /// run() fills state each iteration. The App is itself an asset: a .tapp file IS a
    /// serialized App (config + settings; state stays runtime-only), decoded by load<App>
    /// and re-applied live when the watched file changes.
    struct TBX_API App
    {
        AppState state = {};
        AppConfig config = {};
        AppSettings settings = {};
    };

    /// @brief
    /// Purpose: Decodes a .tapp file into an App (missing keys keep their defaults;
    /// sandbox/icon accept asset paths or uuids).
    template <>
    TBX_API Result<App> load<App>(const std::filesystem::path& path);

    /// @brief
    /// Purpose: Loads the app from its .tapp: load<App> plus the derived paths — the asset
    /// root becomes the .tapp's folder and the file itself is remembered for live re-apply.
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
    /// DirectionalLight, RigidBody, Collider, Script, AudioListener, AudioSource) — THE one
    /// registration call. Idempotent; run() and every subsystem entry point call it, tests may too.
    TBX_API void register_builtin_blocks();

    // The runtime-owned objects run() booted, for hosts and systems (valid between the first
    // run() and the run() that returns false). Everything else — assets, events, jobs,
    // scripts, ui, audio, physics — is a module: call it directly (tbx::assets::load_now,
    // tbx::events::key(), ...).

    /// @brief
    /// Purpose: THE world container.
    TBX_API Sandbox& get_sandbox();

    /// @brief
    /// Purpose: The standard renderer (default pass list; hosts may reshape it).
    TBX_API RenderGraph& get_render_graph();

    /// @brief
    /// Purpose: The OS window.
    TBX_API Window& get_window();
}
