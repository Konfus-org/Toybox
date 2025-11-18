#pragma once
#include "tbx/tbx_api.h"
#include "tbx/std/string.h"
#include <filesystem>
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
        const String name = "";

        // Absolute or relative base directory used to resolve other paths.
        const String working_root = "";

        // Location of runtime assets. May be relative to working_root.
        const String assets_directory = "";

        // Directory where runtime logs should be stored.
        // May be empty to fall back to the working root.
        const String logs_directory = "";

        // Directory searched for plugin binaries and manifests.
        const String plugins_directory = "";

        // Ordered list of plugin identifiers requested for loading.
        const List<String> requested_plugins = {};
    };
}
