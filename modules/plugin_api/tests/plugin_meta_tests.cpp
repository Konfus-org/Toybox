#include "pch.h"
#include "tbx/plugin_api/plugin_meta.h"
#include <string>
#include <vector>

namespace tbx::tests::plugin_api
{
    /// <summary>
    /// Validates that parsing populates each metadata field from the manifest text.
    /// </summary>
    TEST(plugin_meta_parse_test, populates_expected_fields)
    {
        constexpr const char* manifest_text = R"JSON({
                "name": "Example.Logger",
                "version": "1.2.3",
                "description": " Example description ",
                "type": "logger",
                "dependencies": ["Core.Renderer"],
                "module": "bin/logger.so",
                "static": true
            })JSON";
        const FilePath manifest_path = FilePath("/virtual/plugin_api/example/logger/plugin.meta");

        PluginMeta meta =
            parse_plugin_meta(manifest_text, manifest_path);

        EXPECT_EQ(meta.name.std_str(), "Example.Logger");
        EXPECT_EQ(meta.version.std_str(), "1.2.3");
        EXPECT_EQ(meta.description.std_str(), "Example description");
        EXPECT_EQ(meta.type.std_str(), "logger");
        ASSERT_EQ(meta.dependencies.size(), 1u);
        EXPECT_EQ(meta.dependencies[0].std_str(), "Core.Renderer");
        EXPECT_EQ(meta.root_directory, manifest_path.parent_path());
        EXPECT_EQ(meta.manifest_path, manifest_path);
        EXPECT_EQ(meta.module_path, FilePath(manifest_path.parent_path().std_path() / "bin/logger.so"));
        EXPECT_EQ(meta.linkage, PluginLinkage::Static);
    }

    /// <summary>
    /// Confirms the parser assigns the default type when it is absent from the manifest.
    /// </summary>
    TEST(plugin_meta_parse_test, defaults_missing_type_to_plugin)
    {
        constexpr const char* manifest_text = R"JSON({
                "name": "Example.WithoutType",
                "version": "0.1.0",
                "description": "No explicit type set."
            })JSON";
        const FilePath manifest_path = FilePath("/virtual/plugin_api/example/without_type/plugin.meta");

        PluginMeta meta =
            parse_plugin_meta(manifest_text, manifest_path);

        EXPECT_EQ(meta.type.std_str(), "plugin");
    }

    /// <summary>
    /// Validates relative module paths resolve against the manifest directory.
    /// </summary>
    TEST(plugin_meta_parse_test, resolves_relative_module_paths)
    {
        constexpr const char* manifest_text = R"JSON({
                "name": "Example.RelativeModule",
                "version": "5.4.3",
                "type": "renderer",
                "module": "modules/example_renderer.so"
            })JSON";
        const FilePath manifest_path = FilePath("/virtual/plugin_api/example/relative_module/plugin.meta");

        PluginMeta meta =
            parse_plugin_meta(manifest_text, manifest_path);

        EXPECT_EQ(
            meta.module_path,
            FilePath(manifest_path.parent_path().std_path() / "modules/example_renderer.so"));
    }

    /// <summary>
    /// Ensures load and unload ordering respects dependencies and logger priority.
    /// </summary>
    TEST(plugin_meta_load_order_test, prioritizes_logger_and_respects_dependencies)
    {
        PluginMeta logger;
        logger.name = "Logging.Core";
        logger.version = "1.0.0";
        logger.type = "logger";

        PluginMeta metrics;
        metrics.name = "Metrics.Plugin";
        metrics.version = "1.0.0";
        metrics.type = "metrics";
        PluginMeta renderer;
        renderer.name = "Renderer.Plugin";
        renderer.version = "2.0.0";
        renderer.type = "renderer";
        renderer.dependencies.push_back("Metrics.Plugin");

        PluginMeta gameplay;
        gameplay.name = "Gameplay.Plugin";
        gameplay.version = "3.0.0";
        gameplay.type = "gameplay";
        gameplay.dependencies.push_back("metrics");
        gameplay.dependencies.push_back("logger");

        List<PluginMeta> unordered = {gameplay, renderer, metrics, logger};

        List<PluginMeta> load_order = resolve_plugin_load_order(unordered);
        ASSERT_EQ(load_order.size(), 4u);
        EXPECT_EQ(load_order[0].name.std_str(), "Logging.Core");

        auto find_plugin = [](const List<PluginMeta>& plugins, const String& id)
        {
            for (size_t index = 0; index < plugins.size(); index += 1)
            {
                if (plugins[index].name == id)
                {
                    return index;
                }
            }
            return static_cast<size_t>(plugins.size());
        };

        size_t logger_index = find_plugin(load_order, "Logging.Core");
        size_t metrics_index = find_plugin(load_order, "Metrics.Plugin");
        size_t renderer_index = find_plugin(load_order, "Renderer.Plugin");
        size_t gameplay_index = find_plugin(load_order, "Gameplay.Plugin");

        EXPECT_LT(logger_index, metrics_index);
        EXPECT_LT(metrics_index, renderer_index);
        EXPECT_LT(metrics_index, gameplay_index);
        EXPECT_LT(logger_index, gameplay_index);

        List<PluginMeta> unload_order = resolve_plugin_unload_order(unordered);
        ASSERT_EQ(unload_order.size(), load_order.size());
        for (size_t index = 0; index < load_order.size(); index += 1)
        {
            EXPECT_EQ(unload_order[index].name, load_order[load_order.size() - 1 - index].name);
        }
    }
}

