#include "in_memory_file_ops.h"
#include "tbx/systems/app/launch_config.h"
#include <gtest/gtest.h>

namespace tbx::tests::app
{
    TEST(launch_config, reads_valid_config)
    {
        // Arrange
        auto file_ops = InMemoryFileOps("game");
        file_ops.set_text(
            "LaunchConfig.json",
            R"({
                "plugins": ["ThreeDExampleRuntime", "PerformanceMonitor"],
                "settings_asset": "Settings.json",
                "startup_world": 822083584
            })");

        // Act
        const auto read_result = read_launch_config(file_ops);

        // Assert
        ASSERT_TRUE(read_result.result);
        EXPECT_FALSE(read_result.used_default_config);
        ASSERT_EQ(read_result.config.plugins.size(), 2U);
        EXPECT_EQ(read_result.config.plugins[0], "ThreeDExampleRuntime");
        EXPECT_EQ(read_result.config.plugins[1], "PerformanceMonitor");
        EXPECT_EQ(read_result.config.settings_asset, "Settings.json");
        EXPECT_EQ(read_result.config.startup_world.id, Uuid(822083584U));
    }

    TEST(launch_config, uses_defaults_when_config_is_missing)
    {
        // Arrange
        const auto file_ops = InMemoryFileOps("game");

        // Act
        const auto read_result = read_launch_config(file_ops);

        // Assert
        ASSERT_TRUE(read_result.result);
        EXPECT_TRUE(read_result.used_default_config);
        EXPECT_TRUE(read_result.config.plugins.empty());
        EXPECT_EQ(read_result.config.settings_asset, "Settings.json");
        EXPECT_FALSE(read_result.config.startup_world.is_valid());
    }

    TEST(launch_config, rejects_malformed_json)
    {
        // Arrange
        auto file_ops = InMemoryFileOps("game");
        file_ops.set_text("LaunchConfig.json", "{ not json");

        // Act
        const auto read_result = read_launch_config(file_ops);

        // Assert
        EXPECT_FALSE(read_result.result);
        EXPECT_FALSE(read_result.used_default_config);
        EXPECT_FALSE(read_result.result.get_report().empty());
    }

    TEST(launch_config, rejects_wrong_field_types)
    {
        // Arrange
        auto file_ops = InMemoryFileOps("game");
        file_ops.set_text(
            "LaunchConfig.json",
            R"({
                "plugins": ["ThreeDExampleRuntime", 42],
                "settings_asset": false
            })");

        // Act
        const auto read_result = read_launch_config(file_ops);

        // Assert
        EXPECT_FALSE(read_result.result);
        EXPECT_FALSE(read_result.used_default_config);
        EXPECT_FALSE(read_result.result.get_report().empty());
    }

    TEST(launch_config, rejects_wrong_startup_world_type)
    {
        // Arrange
        auto file_ops = InMemoryFileOps("game");
        file_ops.set_text(
            "LaunchConfig.json",
            R"({
                "plugins": ["ThreeDExampleRuntime"],
                "settings_asset": "Settings.json",
                "startup_world": "Example.world"
            })");

        // Act
        const auto read_result = read_launch_config(file_ops);

        // Assert
        EXPECT_FALSE(read_result.result);
        EXPECT_FALSE(read_result.used_default_config);
        EXPECT_FALSE(read_result.result.get_report().empty());
    }
}
