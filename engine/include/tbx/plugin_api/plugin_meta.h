#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace tbx::plugin_api
{
    /// <summary>
    /// Describes the metadata discovered for a plugin before it is loaded.
    /// </summary>
    struct PluginMeta
    {
        /// <summary>
        /// Unique identifier for the plugin used to resolve dependencies.
        /// </summary>
        std::string id;
        /// <summary>
        /// Human-readable name for diagnostic output.
        /// </summary>
        std::string name;
        /// <summary>
        /// Semantic version string reported by the plugin.
        /// </summary>
        std::string version;
        /// <summary>
        /// Exported entry point used to create the plugin instance.
        /// </summary>
        std::string entry_point;
        /// <summary>
        /// Optional descriptive text explaining the plugin purpose.
        /// </summary>
        std::string description;
        /// <summary>
        /// Primary classification for the plugin such as renderer or logger.
        /// </summary>
        std::string type;
        /// <summary>
        /// Hard dependencies that must be satisfied before loading this plugin.
        /// </summary>
        std::vector<std::string> hard_dependencies;
        /// <summary>
        /// Optional dependencies used to improve load ordering when available.
        /// </summary>
        std::vector<std::string> soft_dependencies;
        /// <summary>
        /// Path to the manifest file that produced this metadata.
        /// </summary>
        std::filesystem::path manifest_path;
        /// <summary>
        /// Directory containing the manifest and plugin module.
        /// </summary>
        std::filesystem::path root_directory;
        /// <summary>
        /// Full path to the plugin module that should be loaded.
        /// </summary>
        std::filesystem::path module_path;
    };

    /// <summary>
    /// Parses a plugin manifest from disk and returns the populated metadata.
    /// </summary>
    PluginMeta parse_plugin_meta(const std::filesystem::path& manifest_path);
    /// <summary>
    /// Parses plugin metadata from raw manifest text, primarily for testing.
    /// </summary>
    PluginMeta parse_plugin_meta_text(const std::string& manifest_text, const std::filesystem::path& manifest_path);

    /// <summary>
    /// Orders plugins for loading while respecting dependencies and logger priority.
    /// </summary>
    std::vector<PluginMeta> resolve_plugin_load_order(const std::vector<PluginMeta>& plugins);
    /// <summary>
    /// Produces the unload order by reversing the computed load order.
    /// </summary>
    std::vector<PluginMeta> resolve_plugin_unload_order(const std::vector<PluginMeta>& plugins);
}

