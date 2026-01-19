#pragma once
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

        // Location of runtime assets. Relative to working root.
        const std::filesystem::path assets_directory = {};

        // Directory where runtime logs should be stored relative to working root.
        // May be empty to fall back to the working root.
        const std::filesystem::path logs_directory = {};

        // Directory searched for plugin binaries and manifests, relative to working root.
        const std::filesystem::path plugins_directory = {};

        // Ordered list of plugin identifiers requested for loading.
        const std::vector<std::string> requested_plugins = {};
    };
}
