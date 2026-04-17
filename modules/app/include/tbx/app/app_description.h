#pragma once
#include "tbx/app/app_settings.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    // Immutable application configuration passed to the host on construction.
    struct TBX_API AppDescription
    {
        // Human-readable application name used in logs, manifests, and window names.
        std::string name = "";

        // Args given passed in at start.
        std::vector<std::string> args = {};

        // Absolute or relative base directory used to resolve other paths.
        std::filesystem::path working_root = {};

        // Directory where runtime logs should be stored relative to working root.
        // May be empty to fall back to the working root.
        std::filesystem::path logs_directory = {};

        // Ordered list of plugin identifiers requested for loading.
        std::vector<std::string> requested_plugins = {};

        // Startup icon asset used for native window icons.
        // Defaults to the built-in box icon.
        Handle icon = ToyboxIcon::HANDLE;
    };
}
