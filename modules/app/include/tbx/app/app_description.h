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
    // Ownership: caller owns any referenced filesystem paths; this is a value
    // type copied into the Application.
    // Thread-safety: Treated as read-only data shared on a single thread.
    struct TBX_API AppDescription
    {
        // Human-readable application name used in logs, manifests, and window names.
        const std::string name = "";

        // Absolute or relative base directory used to resolve other paths.
        const std::filesystem::path working_root = {};

        // Directory where runtime logs should be stored relative to working root.
        // May be empty to fall back to the working root.
        const std::filesystem::path logs_directory = {};

        // Async runtime settings used to initialize the application's job manager.
        const AsyncSettings async = {};

        // Ordered list of plugin identifiers requested for loading.
        const std::vector<std::string> requested_plugins = {};

        // Startup icon asset used for native window icons.
        // Defaults to the built-in box icon.
        const Handle icon = box_icon;
    };
}
