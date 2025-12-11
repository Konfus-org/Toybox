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
                "dependencies": ["Core.Renderer"],
                "module": "bin/logger.so",
                "static": true
            })JSON";
        const FilePath manifest_path = FilePath("/virtual/plugin_api/example/logger/plugin.meta");

        PluginMeta meta;
        ASSERT_TRUE(try_parse_plugin_meta(manifest_text, manifest_path, meta));

        EXPECT_EQ(meta.name.std_str(), "Example.Logger");
        EXPECT_EQ(meta.version.std_str(), "1.2.3");
        EXPECT_EQ(meta.description.std_str(), "Example description");
        ASSERT_EQ(meta.dependencies.size(), 1u);
        EXPECT_EQ(meta.dependencies[0].std_str(), "Core.Renderer");
        EXPECT_EQ(meta.root_directory, manifest_path.parent_path());
        EXPECT_EQ(meta.manifest_path, manifest_path);
        EXPECT_EQ(
            meta.module_path,
            FilePath(manifest_path.parent_path().std_path() / "bin/logger.so"));
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
        const FilePath manifest_path =
            FilePath("/virtual/plugin_api/example/relative_module/plugin.meta");

        PluginMeta meta;
        ASSERT_TRUE(try_parse_plugin_meta(manifest_text, manifest_path, meta));

        EXPECT_EQ(
            meta.module_path,
            FilePath(manifest_path.parent_path().std_path() / "modules/example_renderer.so"));
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
        EXPECT_FALSE(try_parse_plugin_meta(manifest_text, FilePath("/virtual/invalid.meta"), meta));
    }
}
