#pragma once
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    // Immutable application configuration passed to the host on construction.
    // Ownership: caller owns any referenced filesystem paths; this is a value
    // type copied into the Application and ApplicationContext.
    // Thread-safety: Treated as read-only data shared on a single thread.
    struct TBX_API AppDescription
    {
        const std::string name = "";
        const std::filesystem::path working_root = "";
        const std::filesystem::path assets_directory = "";
        const std::filesystem::path plugins_directory = "";
        const std::vector<std::string> requested_plugins = {};
    };
}
