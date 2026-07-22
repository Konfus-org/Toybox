#pragma once
#include "tbx/assets/asset.h"
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
#include "tbx/scripting/script.h"
#include "tbx/scripting/scripts.h"
#include "tbx/ui/ui_block.h"
#include "tbx/utils/api.h"
#include "tbx/utils/command_list.h"
#include "tbx/utils/typedefs.h"
#include <filesystem>
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Graphics configuration applied at boot.
    struct TBX_API GraphicsSettings
    {
        bool is_vsync_enabled = false;
        // True = the host owns rendering (run() skips the builtin graph).
        bool is_custom_pipeline = false;
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
    /// Purpose: The app's lifecycle, advanced by run(): CREATED until the first run() boots,
    /// RUNNING through the loop, QUIT_REQUESTED after quit() (or a boot failure), STOPPED
    /// once run() has shut the frame loop down.
    enum class AppStatus : uint8
    {
        CREATED = 0,
        RUNNING,
        QUIT_REQUESTED,
        STOPPED
    };

    /// @brief
    /// Purpose: What the app IS: window shape, content wiring, and where its assets live.
    struct TBX_API AppConfig
    {
        int width = 1600;
        int height = 900;
        bool is_headless = false;
        std::string title = "Toybox";
        AssetHandle<Box> sandbox = {}; // the .box the boot opens
        AssetHandle<Texture> icon = {}; // the window/taskbar icon

        // Derived, never serialized: where assets live — the host sets it (usually the
        // .tapp's folder) and the live value survives .tapp hot reloads. Boot requires it.
        std::filesystem::path root_dir = {};
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
    /// serialized App (config + settings; state stays runtime-only), decoded generically
    /// through its reflected fields — tbx::load<App>(path) after reflection::initialize()
    /// — and re-applied live when the watched file changes.
    struct TBX_API App : Asset
    {
        AppStatus status = AppStatus::CREATED;
        AppConfig config = {};
        AppSettings settings = {};
        // Per-launch, never serialized — the host hands main()'s arguments over and the
        // runtime honors the built-in options (see cmdline_handler.h).
        CommandList commands = {};
    };

}
