#include "pch.h"
#include "tbx/app/application.h"
#include "tbx/debugging/logging.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_loader.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace tbx::tests::plugin_loader
{
    class TestStaticPlugin : public ::tbx::Plugin
    {
      public:
        void on_attach(::tbx::IPluginHost&) override {}
        void on_detach() override {}
        void on_update(const ::tbx::DeltaTime&) override {}
        void on_recieve_message(::tbx::Message&) override {}
    };

    TBX_REGISTER_STATIC_PLUGIN(TestStaticPlugin, TestStaticPlugin)

    static ::tbx::PluginMeta make_static_meta()
    {
        ::tbx::PluginMeta meta;
        meta.name = "TestStaticPlugin";
        meta.version = "1.0.0";
        meta.abi_version = ::tbx::PluginAbiVersion;
        meta.linkage = ::tbx::PluginLinkage::STATIC;
        return meta;
    }

    class TestPluginHost final : public ::tbx::IPluginHost
    {
      public:
        explicit TestPluginHost(const std::filesystem::path& working_directory)
            : _asset_manager(working_directory)
            , _settings(_coordinator, true, ::tbx::GraphicsApi::OPEN_GL, {1280, 720})
        {
            _settings.working_directory = working_directory;
            _settings.logs_directory = working_directory / "logs";
            _settings.plugins_directory = working_directory;
        }

        const std::string& get_name() const override
        {
            return _name;
        }

        ::tbx::AppSettings& get_settings() override
        {
            return _settings;
        }

        ::tbx::IMessageCoordinator& get_message_coordinator() override
        {
            return _coordinator;
        }

        ::tbx::EntityRegistry& get_entity_registry() override
        {
            return _entity_manager;
        }

        ::tbx::AssetManager& get_asset_manager() override
        {
            return _asset_manager;
        }

      private:
        std::string _name = "PluginLoaderTests";
        ::tbx::AppMessageCoordinator _coordinator = {};
        ::tbx::EntityRegistry _entity_manager = {};
        ::tbx::AssetManager _asset_manager;
        ::tbx::AppSettings _settings;
    };

    /// <summary>
    /// Validates that static plugin metadata loads a registered plugin instance.
    /// </summary>
    TEST(plugin_loader, loads_static_plugin_from_meta_list)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_loader";
        TestPluginHost host = TestPluginHost(working_directory);
        const auto metas = std::vector<PluginMeta> {make_static_meta()};

        // Act
        auto loaded = ::tbx::load_plugins(metas, working_directory, host);

        // Assert
        const bool has_fallback_logger =
            ::tbx::PluginRegistry::get_instance().find_plugin(
                std::string(get_stdout_fallback_logger_name()))
            != nullptr;
        const size_t expected_count = has_fallback_logger ? 2u : 1u;
        ASSERT_EQ(loaded.size(), expected_count);
        auto found_test = std::find_if(
            loaded.begin(),
            loaded.end(),
            [](const LoadedPlugin& plugin)
            {
                return plugin.meta.name == "TestStaticPlugin";
            });
        ASSERT_NE(found_test, loaded.end());
        EXPECT_NE(found_test->instance.get(), nullptr);

        // Fallback logging plugin loads only when the static plugin is registered in this binary.
        if (has_fallback_logger)
        {
            auto found_fallback = std::find_if(
                loaded.begin(),
                loaded.end(),
                [](const LoadedPlugin& plugin)
                {
                    return plugin.meta.name == get_stdout_fallback_logger_name();
                });
            ASSERT_NE(found_fallback, loaded.end());
        }
    }

    /// <summary>
    /// Confirms mismatched ABI versions prevent a plugin from loading.
    /// </summary>
    TEST(plugin_loader, rejects_plugin_with_mismatched_abi_version)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_loader";
        TestPluginHost host = TestPluginHost(working_directory);
        auto mismatched = make_static_meta();
        mismatched.abi_version = 77;
        const auto metas = std::vector<PluginMeta> {mismatched};

        // Act
        auto loaded = ::tbx::load_plugins(metas, working_directory, host);

        // Assert
        const bool has_fallback_logger =
            ::tbx::PluginRegistry::get_instance().find_plugin(
                std::string(get_stdout_fallback_logger_name()))
            != nullptr;
        const size_t expected_count = has_fallback_logger ? 1u : 0u;
        ASSERT_EQ(loaded.size(), expected_count);
        if (has_fallback_logger)
        {
            EXPECT_EQ(loaded[0].meta.name, get_stdout_fallback_logger_name());
        }
    }

    /// <summary>
    /// Verifies the loader returns only fallback plugins when no metadata is supplied.
    /// </summary>
    TEST(plugin_loader, loads_fallback_plugins_when_no_metadata_is_provided)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_loader";
        TestPluginHost host = TestPluginHost(working_directory);
        const auto metas = std::vector<PluginMeta> {};

        // Act
        auto loaded = ::tbx::load_plugins(metas, working_directory, host);

        // Assert
        const bool has_fallback_logger =
            ::tbx::PluginRegistry::get_instance().find_plugin(
                std::string(get_stdout_fallback_logger_name()))
            != nullptr;
        const size_t expected_count = has_fallback_logger ? 1u : 0u;
        ASSERT_EQ(loaded.size(), expected_count);
        if (has_fallback_logger)
        {
            EXPECT_EQ(loaded[0].meta.name, get_stdout_fallback_logger_name());
        }
    }
}
