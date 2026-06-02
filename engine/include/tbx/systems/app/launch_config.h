#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/app/launch_config.generated.h"
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include "tbx/utils/result.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Stores host startup configuration that must be available before asset loading.
    /// @details
    /// Ownership: Owns all launch-time configuration values.
    /// Thread Safety: Safe to copy between threads.
    [[serializable]];
    [[version(1U)]];
    struct TBX_API LaunchConfig
    {
        [[prop]]
        std::vector<std::string> plugins = {};

        [[prop]]
        std::string settings_asset = "Settings.json";

        [[prop]]
        Handle startup_world = {};
    };

    /// @brief
    /// Purpose: Carries launch config parsing status and the parsed or default config.
    /// @details
    /// Ownership: Owns the config and status report values.
    /// Thread Safety: Not thread-safe; intended for single-threaded startup.
    struct TBX_API LaunchConfigReadResult
    {
        Result result = {};
        LaunchConfig config = {};
    };

    /// @brief
    /// Purpose: Reads launch-time host config from a JSON file.
    /// @details
    /// Ownership: Does not retain references to file operations or path values.
    /// Thread Safety: Depends on the provided file operations implementation.
    TBX_API LaunchConfigReadResult read_launch_config(
        const IFileOps& file_ops,
        const std::filesystem::path& path = "LaunchConfig.json");
}
