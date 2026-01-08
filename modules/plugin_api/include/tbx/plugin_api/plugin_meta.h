#pragma once
#include "tbx/common/int.h"
#include "tbx/files/filesystem.h"
#include "tbx/plugin_api/plugin_linkage.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tbx
{
    #ifndef TBX_PLUGIN_ABI_VERSION
        #define TBX_PLUGIN_ABI_VERSION 1
    #endif

    /// <summary>
    /// Defines the plugin ABI version enforced by the host.
    /// </summary>
    /// <remarks>
    /// Purpose: Compare manifest ABI versions before loading plugins.
    /// Ownership: Not applicable.
    /// Thread Safety: Immutable constant.
    /// </remarks>
    inline constexpr uint32 PluginAbiVersion = static_cast<uint32>(TBX_PLUGIN_ABI_VERSION);

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

        // ABI version reported by the plugin manifest for compatibility checks.
        uint32 abi_version = PluginAbiVersion;

        PluginLinkage linkage = PluginLinkage::Dynamic;

        // Path to the manifest file that produced this metadata.
        std::filesystem::path manifest_path;

        // Directory containing the manifest and plugin module.
        std::filesystem::path root_directory;

        // Full path to the plugin library that should be loaded.
        std::filesystem::path library_path;
    };

    class TBX_API PluginMetaParser
    {
      public:
        // Parses a plugin manifest from disk and returns the populated metadata.
        // Returns true when the manifest could be parsed successfully.
        bool try_parse_plugin_meta(
            const IFileSystem& fs,
            const std::filesystem::path& manifest_path,
            PluginMeta& out_meta);

        // Parses plugin metadata from raw manifest text. Returns true when parsing succeeds.
        bool try_parse_plugin_meta(
            std::string_view manifest_text,
            const std::filesystem::path& manifest_path,
            PluginMeta& out_meta);
    };
}
