#include "pch.h"
#include "tbx/plugin_api/plugin_meta.h"
#include <filesystem>
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
                "id": "Example.Logger",
                "name": "Example Logger",
                "version": "1.2.3",
                "entryPoint": "ExampleEntry",
                "description": " Example description ",
                "type": "logger",
                "dependencies": ["Core.Renderer"],
                "module": "bin/logger.so",
                "static": true
            })JSON";
        const std::filesystem::path manifest_path = "/virtual/plugin_api/example/logger/plugin.meta";

        ::tbx::PluginMeta meta =
            ::tbx::parse_plugin_meta(manifest_text, manifest_path);

        EXPECT_EQ(meta.id, "Example.Logger");
        EXPECT_EQ(meta.name, "Example Logger");
        EXPECT_EQ(meta.version, "1.2.3");
        EXPECT_EQ(meta.entry_point, "ExampleEntry");
        EXPECT_EQ(meta.description, "Example description");
        EXPECT_EQ(meta.type, "logger");
        ASSERT_EQ(meta.dependencies.size(), 1u);
        EXPECT_EQ(meta.dependencies[0], "Core.Renderer");
        EXPECT_EQ(meta.root_directory, manifest_path.parent_path());
        EXPECT_EQ(meta.manifest_path, manifest_path);
        EXPECT_EQ(meta.module_path, manifest_path.parent_path() / "bin/logger.so");
        EXPECT_EQ(meta.linkage, ::tbx::PluginLinkage::Static);
    }

    /// <summary>
    /// Confirms the parser assigns the default type when it is absent from the manifest.
    /// </summary>
    TEST(plugin_meta_parse_test, defaults_missing_type_to_plugin)
    {
        constexpr const char* manifest_text = R"JSON({
                "id": "Example.WithoutType",
                "name": "Example Without Type",
                "version": "0.1.0",
                "entryPoint": "ExampleEntry",
                "description": "No explicit type set."
            })JSON";
        const std::filesystem::path manifest_path = "/virtual/plugin_api/example/without_type/plugin.meta";

        ::tbx::PluginMeta meta =
            ::tbx::parse_plugin_meta(manifest_text, manifest_path);

        EXPECT_EQ(meta.type, "plugin");
    }

    /// <summary>
    /// Validates relative module paths resolve against the manifest directory.
    /// </summary>
    TEST(plugin_meta_parse_test, resolves_relative_module_paths)
    {
        constexpr const char* manifest_text = R"JSON({
                "id": "Example.RelativeModule",
                "name": "Example Relative Module",
                "version": "5.4.3",
                "entryPoint": "ExampleEntry",
                "type": "renderer",
                "module": "modules/example_renderer.so"
            })JSON";
        const std::filesystem::path manifest_path = "/virtual/plugin_api/example/relative_module/plugin.meta";

        ::tbx::PluginMeta meta =
            ::tbx::parse_plugin_meta(manifest_text, manifest_path);

        EXPECT_EQ(meta.module_path, manifest_path.parent_path() / "modules/example_renderer.so");
    }

    /// <summary>
    /// Ensures load and unload ordering respects dependencies and logger priority.
    /// </summary>
    TEST(plugin_meta_load_order_test, prioritizes_logger_and_respects_dependencies)
    {
        ::tbx::PluginMeta logger;
        logger.id = "Logging.Core";
        logger.name = "Logging";
        logger.version = "1.0.0";
        logger.entry_point = "LoggingEntry";
        logger.type = "logger";

        ::tbx::PluginMeta metrics;
        metrics.id = "Metrics.Plugin";
        metrics.name = "Metrics";
        metrics.version = "1.0.0";
        metrics.entry_point = "MetricsEntry";
        metrics.type = "metrics";
        ::tbx::PluginMeta renderer;
        renderer.id = "Renderer.Plugin";
        renderer.name = "Renderer";
        renderer.version = "2.0.0";
        renderer.entry_point = "RendererEntry";
        renderer.type = "renderer";
        renderer.dependencies.push_back("Metrics.Plugin");

        ::tbx::PluginMeta gameplay;
        gameplay.id = "Gameplay.Plugin";
        gameplay.name = "Gameplay";
        gameplay.version = "3.0.0";
        gameplay.entry_point = "GameplayEntry";
        gameplay.type = "gameplay";
        gameplay.dependencies.push_back("metrics");
        gameplay.dependencies.push_back("logger");

        std::vector<::tbx::PluginMeta> unordered = {gameplay, renderer, metrics, logger};

        std::vector<::tbx::PluginMeta> load_order = ::tbx::resolve_plugin_load_order(unordered);
        ASSERT_EQ(load_order.size(), 4u);
        EXPECT_EQ(load_order[0].id, "Logging.Core");

        auto find_plugin = [](const std::vector<::tbx::PluginMeta>& plugins, const std::string& id)
        {
            for (size_t index = 0; index < plugins.size(); index += 1)
            {
                if (plugins[index].id == id)
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

        std::vector<::tbx::PluginMeta> unload_order = ::tbx::resolve_plugin_unload_order(unordered);
        ASSERT_EQ(unload_order.size(), load_order.size());
        for (size_t index = 0; index < load_order.size(); index += 1)
        {
            EXPECT_EQ(unload_order[index].id, load_order[load_order.size() - 1 - index].id);
        }
    }
}

