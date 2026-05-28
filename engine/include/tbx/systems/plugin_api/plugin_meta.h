#pragma once
#include "tbx/systems/plugin_api/plugin_linkage.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <filesystem>
#include <string>
#include <vector>

#ifndef TBX_PLUGIN_ABI_VERSION
    #define TBX_PLUGIN_ABI_VERSION 1
#endif

namespace tbx
{
    /// @brief
    /// Purpose: Compare generated plugin ABI versions before loading plugins.
    /// @details
    /// Ownership: Not applicable.
    /// Thread Safety: Immutable constant.
    inline constexpr uint32 PluginAbiVersion = static_cast<uint32>(TBX_PLUGIN_ABI_VERSION);

    /// @brief
    /// Purpose: Describe broad update phases that the host can use when ordering plugin updates.
    /// @details
    /// Ownership: Not applicable.
    /// Thread Safety: Immutable enum values.
    enum class PluginCategory : uint32
    {
        DEFAULT = 0,
        LOGGING = 50,
        INPUT = 100,
        AUDIO = 200,
        PHYSICS = 300,
        RENDERING = 400,
        GAMEPLAY = 500
    };

    // Describes the metadata discovered for a plugin before it is loaded.
    struct TBX_API PluginMeta
    {
        // Unique identifier for the plugin used to resolve dependencies and lookup.
        std::string name;

        // Semantic version string reported by the plugin.
        std::string version;

        // Optional descriptive text explaining the plugin purpose.
        std::string description;

        // Hard dependencies that must be satisfied before loading this plugin.
        std::vector<std::string> dependencies;

        // Optional directory that should be searched for plugin assets/resources.
        std::filesystem::path resource_directory;

        // ABI version reported by the plugin module for compatibility checks.
        uint32 abi_version = PluginAbiVersion;

        // Broad update phase used when ordering plugin updates.
        PluginCategory category = PluginCategory::DEFAULT;

        // Type of plugin: static/dynamic
        PluginLinkage linkage = PluginLinkage::DYNAMIC;

        // Explicit update priority within the update category (lower values update first).
        uint32 priority = 0;

        // Directory containing the plugin module.
        std::filesystem::path root_directory;

        // Full path to the plugin library that should be loaded.
        std::filesystem::path library_path;
    };

}
