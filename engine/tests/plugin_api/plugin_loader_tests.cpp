#include "pch.h"
#include "tbx/systems/plugin_api/plugin_loader.h"
#include <filesystem>
#include <vector>

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
