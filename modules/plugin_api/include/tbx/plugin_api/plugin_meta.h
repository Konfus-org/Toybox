#pragma once
#include "tbx/common/collections.h"
#include "tbx/common/string.h"
#include "tbx/file_system/filepath.h"
#include "tbx/plugin_api/plugin_linkage.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // Describes the metadata discovered for a plugin before it is loaded.
    struct TBX_API PluginMeta
    {
        // Unique identifier for the plugin used to resolve dependencies and lookup.
        String name;

        // Semantic version string reported by the plugin.
        String version;

        // Optional descriptive text explaining the plugin purpose.
        String description;

        // Primary classification for the plugin such as renderer or logger.
        String type;

        // Hard dependencies that must be satisfied before loading this plugin.
        List<String> dependencies;

        PluginLinkage linkage = PluginLinkage::Dynamic;

        // Path to the manifest file that produced this metadata.
        FilePath manifest_path;

        // Directory containing the manifest and plugin module.
        FilePath root_directory;

        // Full path to the plugin module that should be loaded.
        FilePath module_path;
    };

    // Parses a plugin manifest from disk and returns the populated metadata.
    PluginMeta TBX_API parse_plugin_meta(const FilePath& manifest_path);

    // Parses plugin metadata from raw manifest text.
    PluginMeta TBX_API parse_plugin_meta(
        const String& manifest_text,
        const FilePath& manifest_path);

    // Orders plugins for loading while respecting dependencies and logger priority.
    List<PluginMeta> TBX_API resolve_plugin_load_order(
        const List<PluginMeta>& plugins);

    // Produces the unload order by reversing the computed load order.
    List<PluginMeta> TBX_API resolve_plugin_unload_order(
        const List<PluginMeta>& plugins);
}
