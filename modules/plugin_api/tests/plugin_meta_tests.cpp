#include "pch.h"
#include "tbx/plugin_api/plugin_meta.h"
#include <filesystem>

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
                "abi_version": 1,
                "description": " Example description ",
                "dependencies": ["Core.Renderer"],
                "category": "audio",
                "priority": 250,
                "module": "bin/logger.so",
                "static": true
            })JSON";
        const std::filesystem::path manifest_path =
            "/virtual/plugin_api/example/logger/plugin.meta";

        PluginMeta meta;
        PluginMetaParser parser;
        ASSERT_TRUE(parser.try_parse_plugin_meta(manifest_text, manifest_path, meta));

        EXPECT_EQ(meta.name, "Example.Logger");
        EXPECT_EQ(meta.version, "1.2.3");
        EXPECT_EQ(meta.abi_version, PluginAbiVersion);
        EXPECT_EQ(meta.description, "Example description");
        ASSERT_EQ(meta.dependencies.size(), 1u);
        EXPECT_EQ(meta.dependencies[0], "Core.Renderer");
        EXPECT_EQ(meta.category, PluginCategory::Audio);
        EXPECT_EQ(meta.priority, 250u);
        EXPECT_EQ(meta.root_directory, manifest_path.parent_path());
        EXPECT_EQ(meta.manifest_path, manifest_path);
        EXPECT_EQ(
            meta.library_path,
            manifest_path.parent_path() / "bin/logger.so");
        EXPECT_EQ(meta.linkage, PluginLinkage::Static);
    }

    /// <summary>
    /// Validates relative module paths resolve against the manifest directory.
    /// </summary>
    TEST(plugin_meta_parse_test, resolves_relative_module_paths)
    {
        constexpr const char* manifest_text = R"JSON({
                "name": "Example.RelativeModule",
                "version": "5.4.3",
                "module": "modules/example_renderer.so"
            })JSON";
        const std::filesystem::path manifest_path =
            "/virtual/plugin_api/example/relative_module/plugin.meta";

        PluginMeta meta;
        PluginMetaParser parser;
        ASSERT_TRUE(parser.try_parse_plugin_meta(manifest_text, manifest_path, meta));

        EXPECT_EQ(
            meta.library_path,
            manifest_path.parent_path() / "modules/example_renderer.so");
    }

    /// <summary>
    /// Verifies parsing fails when required fields are missing.
    /// </summary>
    TEST(plugin_meta_parse_test, fails_without_required_fields)
    {
        constexpr const char* manifest_text = R"JSON({
                "description": "Missing name and version"
            })JSON";

        PluginMeta meta;
        PluginMetaParser parser;
        EXPECT_FALSE(parser.try_parse_plugin_meta(manifest_text, "/virtual/invalid.meta", meta));
    }
}
