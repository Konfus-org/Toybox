#include "tbx/systems/plugin_api/plugin_loader.h"

namespace tbx::tests::plugin_loader
{
    static ::tbx::PluginMeta make_dynamic_meta()
    {
        ::tbx::PluginMeta meta;
        meta.name = "TestDynamicPlugin";
        meta.version = "1.0.0";
        meta.abi_version = ::tbx::PluginAbiVersion;
        meta.linkage = ::tbx::PluginLinkage::DYNAMIC;
        meta.library_path = "/virtual/plugin_loader/TestDynamicPlugin.dll";
        return meta;
    }

    TEST(plugin_loader, returns_empty_when_no_metadata_is_provided)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_loader";
        const auto metas = std::vector<PluginMeta> {};

        // Act
        auto loaded = ::tbx::load_plugins(metas, working_directory);

        // Assert
        ASSERT_TRUE(loaded.empty());
    }

    TEST(plugin_loader, recognizes_platform_library_paths)
    {
        // Arrange / Act / Assert
#if defined(TBX_PLATFORM_WINDOWS)
        EXPECT_TRUE(::tbx::is_plugin_library_path("ExamplePlugin.dll"));
        EXPECT_FALSE(::tbx::is_plugin_library_path("ExamplePlugin.dll.meta"));
#elif defined(TBX_PLATFORM_MACOS)
        EXPECT_TRUE(::tbx::is_plugin_library_path("libExamplePlugin.dylib"));
        EXPECT_FALSE(::tbx::is_plugin_library_path("libExamplePlugin.dylib.meta"));
#else
        EXPECT_TRUE(::tbx::is_plugin_library_path("libExamplePlugin.so"));
        EXPECT_FALSE(::tbx::is_plugin_library_path("libExamplePlugin.so.meta"));
#endif
    }

    TEST(plugin_loader, rejects_plugin_with_mismatched_abi_version)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_loader";
        auto mismatched = make_dynamic_meta();
        mismatched.abi_version = 77;
        const auto metas = std::vector<PluginMeta> {mismatched};

        // Act
        auto loaded = ::tbx::load_plugins(metas, working_directory);

        // Assert
        ASSERT_TRUE(loaded.empty());
    }
    TEST(plugin_loader, returns_empty_when_dynamic_module_cannot_be_loaded)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_loader";
        const auto metas = std::vector<PluginMeta> {make_dynamic_meta()};

        // Act
        auto loaded = ::tbx::load_plugins(metas, working_directory);

        // Assert
        ASSERT_TRUE(loaded.empty());
    }
}
