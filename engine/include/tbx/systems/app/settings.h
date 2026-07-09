#pragma once
#include "tbx/systems/app/settings.generated.h"
#include "tbx/systems/async/settings.h"
#include "tbx/systems/world/settings.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/physics/settings.h"
#include "tbx/tbx_api.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/assets/builtin_assets.h"
#include "tbx/types/handle.h"
#include <string>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Stores mutable runtime settings for the application host.
    /// @details
    /// Ownership: Owns all stored settings values.
    /// Thread Safety: Not thread-safe; synchronize access externally.
    [[serializable]];
    [[version(1U)]];
    struct TBX_API AppSettings : public Asset
    {
        GraphicsSettings graphics = {};

        WorldSettings world = {};

        PhysicsSettings physics = {};

        AsyncSettings async = {};

        Handle icon = tbx::ToyboxIconTexture::HANDLE;

        // Input map assets whose schemes are fed to the InputManager at startup (and re-fed on a
        // live settings reload), so an app's keybindings are data instead of code.
        std::vector<Handle> input_maps = {};

        std::string name = "Toybox App";

        std::vector<std::string> plugins = {
            "PerformanceMonitor",
            "SdlInput",
            "JoltPhysics",
            "SdlWindowing",
            "SdlOpenGlContextManager",
            "OpenGlRendering",
            "StbImageLoader",
            "AssimpModelLoader",
            "ShaderIncludeLoader",
        };
    };

    inline std::vector<std::string> resolve_app_plugins(
        const std::vector<std::string>& settings_plugins,
        const std::vector<std::string>& command_plugins)
    {
        if (!command_plugins.empty())
            return command_plugins;

        return settings_plugins;
    }
}
