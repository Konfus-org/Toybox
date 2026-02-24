#include "pch.h"
#include "tbx/app/application.h"
#include "tbx/plugin_api/plugin_loader.h"
#include <filesystem>
#include <string>
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

    class TestPluginHost final : public ::tbx::IPluginHost
    {
      public:
        explicit TestPluginHost(const std::filesystem::path& working_directory)
            : _asset_manager(working_directory)
            , _settings(_coordinator, true, ::tbx::GraphicsApi::OPEN_GL, {1280, 720})
        {
            _settings.paths.working_directory = working_directory;
            _settings.paths.logs_directory = working_directory / "logs";
        }

        const std::string& get_name() const override
        {
            return _name;
        }

        const ::tbx::Handle& get_icon_handle() const override
        {
            return _icon_handle;
        }

        ::tbx::AppSettings& get_settings() override
        {
            return _settings;
        }

        ::tbx::IMessageCoordinator& get_message_coordinator() override
        {
            return _coordinator;
        }

        ::tbx::InputManager& get_input_manager() override
        {
            return _input_manager;
        }

        ::tbx::EntityRegistry& get_entity_registry() override
        {
            return _ent_registry;
        }

        ::tbx::AssetManager& get_asset_manager() override
        {
            return _asset_manager;
        }

        ::tbx::JobSystem& get_job_system() override
        {
            return _job_manager;
        }

        ::tbx::ThreadManager& get_thread_manager() override
        {
            return _thread_manager;
        }

      private:
        std::string _name = "PluginLoaderTests";
        ::tbx::Handle _icon_handle = ::tbx::box_icon;
        ::tbx::AppMessageCoordinator _coordinator = {};
        ::tbx::InputManager _input_manager = _coordinator;
        ::tbx::EntityRegistry _ent_registry = {};
        ::tbx::AssetManager _asset_manager;
        ::tbx::AppSettings _settings;
        ::tbx::JobSystem _job_manager;
        ::tbx::ThreadManager _thread_manager;
    };

    /// <summary>
    /// Validates no plugins are loaded when no metadata is provided.
    /// </summary>
    TEST(plugin_loader, returns_empty_when_no_metadata_is_provided)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_loader";
        TestPluginHost host = TestPluginHost(working_directory);
        const auto metas = std::vector<PluginMeta> {};

        // Act
        auto loaded = ::tbx::load_plugins(metas, working_directory, host);

        // Assert
        ASSERT_TRUE(loaded.empty());
    }

    /// <summary>
    /// Confirms mismatched ABI versions prevent a plugin from loading.
    /// </summary>
    TEST(plugin_loader, rejects_plugin_with_mismatched_abi_version)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_loader";
        TestPluginHost host = TestPluginHost(working_directory);
        auto mismatched = make_dynamic_meta();
        mismatched.abi_version = 77;
        const auto metas = std::vector<PluginMeta> {mismatched};

        // Act
        auto loaded = ::tbx::load_plugins(metas, working_directory, host);

        // Assert
        ASSERT_TRUE(loaded.empty());
    }

    /// <summary>
    /// Verifies load failures return an empty result when a dynamic plugin cannot be opened.
    /// </summary>
    TEST(plugin_loader, returns_empty_when_dynamic_module_cannot_be_loaded)
    {
        // Arrange
        const std::filesystem::path working_directory = "/virtual/plugin_loader";
        TestPluginHost host = TestPluginHost(working_directory);
        const auto metas = std::vector<PluginMeta> {make_dynamic_meta()};

        // Act
        auto loaded = ::tbx::load_plugins(metas, working_directory, host);

        // Assert
        ASSERT_TRUE(loaded.empty());
    }
}
