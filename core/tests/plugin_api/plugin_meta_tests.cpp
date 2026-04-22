#include "pch.h"
#include "tbx/core/systems/plugin_api/plugin_meta.h"
#include <filesystem>

namespace tbx::tests::plugin_api
{
    TEST(plugin_meta_parse_test, populates_expected_fields)
    {
        // Arrange
        constexpr const char* manifest_text = R"JSON({
                "name": "Example.Logger",
                "version": "1.2.3",
                "abi_version": 1,
                "description": " Example description ",
                "dependencies": ["Core.Renderer"],
                "resources": ["./assets"],
                "category": "audio",
                "priority": 250,
                "module": "bin/logger.so"
            })JSON";
        const std::filesystem::path manifest_path =
            "/virtual/plugin_api/example/logger/plugin.meta";

        PluginMeta meta;
        PluginMetaParser parser;

        // Act
        ASSERT_TRUE(parser.try_parse_from_source(manifest_text, manifest_path, meta));

        // Assert
        EXPECT_EQ(meta.name, "Example.Logger");
        EXPECT_EQ(meta.version, "1.2.3");
        EXPECT_EQ(meta.abi_version, PluginAbiVersion);
        EXPECT_EQ(meta.description, "Example description");
        ASSERT_EQ(meta.dependencies.size(), 1u);
        EXPECT_EQ(meta.dependencies[0], "Core.Renderer");
        EXPECT_EQ(meta.resource_directory, manifest_path.parent_path() / "assets");
        EXPECT_EQ(meta.category, PluginCategory::AUDIO);
        EXPECT_EQ(meta.priority, 250u);
        EXPECT_EQ(meta.root_directory, manifest_path.parent_path());
        EXPECT_EQ(meta.manifest_path, manifest_path);
        EXPECT_EQ(meta.library_path, manifest_path.parent_path() / "bin/logger.so");
        EXPECT_EQ(meta.linkage, PluginLinkage::DYNAMIC);
    }
    TEST(plugin_meta_parse_test, resolves_relative_module_paths)
    {
        // Arrange
        constexpr const char* manifest_text = R"JSON({
                "name": "Example.RelativeModule",
                "version": "5.4.3",
                "module": "modules/example_renderer.so",
                "resources": ["./assets"]
            })JSON";
        const std::filesystem::path manifest_path =
            "/virtual/plugin_api/example/relative_module/plugin.meta";

        PluginMeta meta;
        PluginMetaParser parser;

        // Act
        ASSERT_TRUE(parser.try_parse_from_source(manifest_text, manifest_path, meta));

        // Assert
        EXPECT_EQ(meta.library_path, manifest_path.parent_path() / "modules/example_renderer.so");
        EXPECT_EQ(meta.resource_directory, manifest_path.parent_path() / "assets");
    }
    TEST(plugin_meta_parse_test, accepts_string_resource_directory)
    {
        // Arrange
        constexpr const char* manifest_text = R"JSON({
                "name": "Example.StringResources",
                "version": "0.1.0",
                "resources": "./assets"
            })JSON";

        const std::filesystem::path manifest_path =
            "/virtual/plugin_api/example/string_resources/plugin.meta";

        PluginMeta meta;
        PluginMetaParser parser;

        // Act
        ASSERT_TRUE(parser.try_parse_from_source(manifest_text, manifest_path, meta));

        // Assert
        EXPECT_EQ(meta.resource_directory, manifest_path.parent_path() / "assets");
    }
    TEST(plugin_meta_parse_test, fails_with_multiple_resource_directories)
    {
        // Arrange
        constexpr const char* manifest_text = R"JSON({
                "name": "Example.MultipleResources",
                "version": "0.1.0",
                "resources": ["./assets", "/tmp/extra_assets"]
            })JSON";

        PluginMeta meta;
        PluginMetaParser parser;

        // Act
        EXPECT_FALSE(parser.try_parse_from_source(manifest_text, "/virtual/multi.meta", meta));
    }
    TEST(plugin_meta_parse_test, fails_without_required_fields)
    {
        // Arrange
        constexpr const char* manifest_text = R"JSON({
                "description": "Missing name and version"
            })JSON";

        PluginMeta meta;
        PluginMetaParser parser;

        // Act
        EXPECT_FALSE(parser.try_parse_from_source(manifest_text, "/virtual/invalid.meta", meta));
    }
    TEST(plugin_meta_parse_test, rejects_static_linkage)
    {
        // Arrange
        constexpr const char* manifest_text = R"JSON({
                "name": "Example.StaticPlugin",
                "version": "1.0.0",
                "static": true
            })JSON";

        PluginMeta meta;
        PluginMetaParser parser;

        // Act
        EXPECT_FALSE(parser.try_parse_from_source(manifest_text, "/virtual/static.meta", meta));
    }
}
