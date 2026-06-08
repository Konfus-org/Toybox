#include "in_memory_file_ops.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/messaging/message_coordinator.h"

namespace tbx::tests::app
{
    static AssetManager make_settings_asset_manager(
        const std::filesystem::path& working_directory,
        const std::shared_ptr<IFileOps>& file_ops)
    {
        auto dispatcher = std::make_shared<MessageCoordinator>();
        auto serialization_registry = std::make_shared<SerializationRegistry>(file_ops);
        return AssetManager(
            dispatcher,
            serialization_registry,
            working_directory,
            std::vector<std::filesystem::path> {},
            {},
            file_ops);
    }

    TEST(app_settings, loads_default_plugins_when_settings_body_omits_plugin_list)
    {
        // Arrange
        const auto working_directory = std::filesystem::path("/virtual/app_settings/defaults");
        auto file_ops = std::make_shared<InMemoryFileOps>(working_directory);
        file_ops->set_text(
            "Settings.json",
            R"({
                "name": "DefaultPluginsApp"
            })");
        auto asset_manager = make_settings_asset_manager(working_directory, file_ops);

        // Act
        const auto settings = asset_manager.load<AppSettings>(Handle("Settings.json"));

        // Assert
        ASSERT_NE(settings, nullptr);
        EXPECT_FALSE(settings->plugins.empty());
    }

    TEST(app_settings, loads_explicit_plugins_from_settings_body_instead_of_defaults)
    {
        // Arrange
        const auto working_directory = std::filesystem::path("/virtual/app_settings/explicit");
        auto file_ops = std::make_shared<InMemoryFileOps>(working_directory);
        file_ops->set_text(
            "Settings.json",
            R"({
                "name": "ExplicitPluginsApp",
                "plugins": [
                    "CustomRendererPlugin",
                    "CustomInputPlugin"
                ]
            })");
        auto asset_manager = make_settings_asset_manager(working_directory, file_ops);

        // Act
        const auto settings = asset_manager.load<AppSettings>(Handle("Settings.json"));

        // Assert
        ASSERT_NE(settings, nullptr);
        ASSERT_EQ(settings->plugins.size(), 2U);
        EXPECT_EQ(settings->plugins[0], "CustomRendererPlugin");
        EXPECT_EQ(settings->plugins[1], "CustomInputPlugin");
    }

    TEST(app_settings, command_line_plugins_override_settings_plugins)
    {
        // Arrange
        const auto settings_plugins =
            std::vector<std::string> {"SettingsRendererPlugin", "SettingsInputPlugin"};
        const auto command_plugins =
            std::vector<std::string> {"CommandRendererPlugin", "CommandInputPlugin"};

        // Act
        const auto resolved_plugins = resolve_app_plugins(settings_plugins, command_plugins);

        // Assert
        ASSERT_EQ(resolved_plugins.size(), 2U);
        EXPECT_EQ(resolved_plugins[0], "CommandRendererPlugin");
        EXPECT_EQ(resolved_plugins[1], "CommandInputPlugin");
    }

    TEST(app_settings, settings_plugins_are_used_when_command_line_plugins_are_missing)
    {
        // Arrange
        const auto settings_plugins =
            std::vector<std::string> {"SettingsRendererPlugin", "SettingsInputPlugin"};
        const auto command_plugins = std::vector<std::string> {};

        // Act
        const auto resolved_plugins = resolve_app_plugins(settings_plugins, command_plugins);

        // Assert
        ASSERT_EQ(resolved_plugins.size(), 2U);
        EXPECT_EQ(resolved_plugins[0], "SettingsRendererPlugin");
        EXPECT_EQ(resolved_plugins[1], "SettingsInputPlugin");
    }
}
