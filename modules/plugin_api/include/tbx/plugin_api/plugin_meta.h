#pragma once
#include "tbx/plugin_api/plugin_linkage.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx
{
    // Describes the metadata discovered for a plugin before it is loaded.
    struct TBX_API PluginMeta
    {
        // Unique identifier for the plugin used to resolve dependencies and lookup.
        std::string name;

        // Semantic version string reported by the plugin.
        std::string version;

        // Optional descriptive text explaining the plugin purpose.
        std::string description;

        // Primary classification for the plugin such as renderer or logger.
        std::string type;

        // Hard dependencies that must be satisfied before loading this plugin.
        std::vector<std::string> dependencies;

        PluginLinkage linkage = PluginLinkage::Dynamic;

        // Path to the manifest file that produced this metadata.
        std::filesystem::path manifest_path;

        // Directory containing the manifest and plugin module.
        std::filesystem::path root_directory;

        // Full path to the plugin module that should be loaded.
        std::filesystem::path module_path;
    };

    // Parses a plugin manifest from disk and returns the populated metadata.
    PluginMeta TBX_API parse_plugin_meta(const std::filesystem::path& manifest_path);

    // Parses plugin metadata from raw manifest text.
    PluginMeta TBX_API parse_plugin_meta(
        const std::string& manifest_text,
        const std::filesystem::path& manifest_path);

    // Orders plugins for loading while respecting dependencies and logger priority.
    std::vector<PluginMeta> TBX_API resolve_plugin_load_order(
        const std::vector<PluginMeta>& plugins);

    // Produces the unload order by reversing the computed load order.
    std::vector<PluginMeta> TBX_API resolve_plugin_unload_order(
        const std::vector<PluginMeta>& plugins);
}
