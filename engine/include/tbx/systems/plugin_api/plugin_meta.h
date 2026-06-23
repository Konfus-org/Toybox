#pragma once
#include "tbx/systems/plugin_api/plugin_meta.generated.h"
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

    [[serializable]];
    [[printable]];
    enum class PluginLinkage
    {
        DYNAMIC [[name("dynamic")]],
        // STATIC <- Currently unsupported!
    };

    /// @brief
    /// Purpose: Describe broad update phases that the host can use when ordering plugin updates.
    /// @details
    /// Ownership: Not applicable.
    /// Thread Safety: Immutable enum values.
    [[serializable]];
    [[printable]];
    enum class PluginCategory : uint32
    {
        DEFAULT [[name("default")]] = 0,
        LOGGING [[name("logging")]] = 50,
        INPUT [[name("input")]] = 100,
        AUDIO [[name("audio")]] = 200,
        PHYSICS [[name("physics")]] = 300,
        RENDERING [[name("rendering")]] = 400,
        SCRIPTING [[name("scripting")]] = 450,
        GAMEPLAY [[name("gameplay")]] = 500
    };

    // Describes the metadata discovered for a plugin before it is loaded.
    [[serializable]];
    [[version(1U)]];
    struct TBX_API PluginMeta
    {
        // Unique identifier for the plugin used to resolve dependencies and lookup.
        [[prop]]
        std::string name;

        // Semantic version string reported by the plugin.
        [[prop]]
        std::string version;

        // Optional descriptive text explaining the plugin purpose.
        [[prop]]
        std::string description;

        // Hard dependencies that must be satisfied before loading this plugin.
        [[prop]]
        std::vector<std::string> dependencies;

        // Optional directory that should be searched for plugin assets/resources.
        [[prop]]
        std::filesystem::path resource_directory;

        // ABI version reported by the plugin module for compatibility checks.
        [[prop]]
        uint32 abi_version = PluginAbiVersion;

        // Broad update phase used when ordering plugin updates.
        [[prop]]
        PluginCategory category = PluginCategory::DEFAULT;

        // Type of plugin: static/dynamic
        [[prop]]
        PluginLinkage linkage = PluginLinkage::DYNAMIC;

        // Explicit update priority within the update category (lower values update first).
        [[prop]]
        uint32 priority = 0;

        // Directory containing the plugin module.
        std::filesystem::path root_directory;

        // Full path to the plugin library that should be loaded.
        std::filesystem::path library_path;
    };

}
