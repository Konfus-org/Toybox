#include "tbx/app.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace tbx::tests
{
    TEST(App, TappFileDeserializesIntoTheAppStruct)
    {
        // Arrange: a .tapp with every settings group, handles authored as paths.
        const auto config_root = std::filesystem::temp_directory_path() / "tbx_app_test";
        std::filesystem::create_directories(config_root);
        const auto tapp = config_root / "Game.tapp";
        {
            auto file = std::ofstream(tapp);
            file << R"({
                "title": "Configured",
                "width": 640,
                "height": 480,
                "sandbox": "levels/entry.box",
                "icon": "textures/icon.png",
                "graphics": {"is_vsync_enabled": false, "shadow_resolution": 1024},
                "physics": {"fixed_timestep": 0.02, "gravity": [0.0, -3.7, 0.0]},
                "audio": {"master_volume": 0.5},
                "assets": {"idle_lifetime_seconds": 5.0}
            })";
        }

        // Act
        const auto loaded = load_app(tapp);

        // Assert: fields land, handles carry authoring paths, root is the .tapp's folder.
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        const App& app = *loaded;
        EXPECT_EQ(app.title, "Configured");
        EXPECT_EQ(app.width, 640);
        EXPECT_EQ(app.height, 480);
        EXPECT_EQ(app.sandbox.path, "levels/entry.box");
        EXPECT_EQ(app.icon.path, "textures/icon.png");
        EXPECT_FALSE(app.graphics.is_vsync_enabled);
        EXPECT_EQ(app.graphics.shadow_resolution, 1024);
        EXPECT_NEAR(app.physics.fixed_timestep, 0.02f, 0.0001f);
        EXPECT_NEAR(app.physics.gravity.y, -3.7f, 0.0001f);
        EXPECT_NEAR(app.audio.master_volume, 0.5f, 0.0001f);
        EXPECT_NEAR(app.assets.idle_lifetime_seconds, 5.0f, 0.0001f);
        EXPECT_EQ(app.asset_root, config_root);
    }

    TEST(App, MissingTappKeysKeepDefaults)
    {
        // Arrange
        const auto config_root = std::filesystem::temp_directory_path() / "tbx_app_test";
        std::filesystem::create_directories(config_root);
        const auto tapp = config_root / "Sparse.tapp";
        {
            auto file = std::ofstream(tapp);
            file << R"({"title": "Sparse"})";
        }

        // Act
        const auto loaded = load_app(tapp);

        // Assert
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        EXPECT_EQ(loaded->title, "Sparse");
        EXPECT_EQ(loaded->width, 1600);
        EXPECT_TRUE(loaded->graphics.is_vsync_enabled);
        EXPECT_NEAR(loaded->physics.fixed_timestep, 1.0f / 60.0f, 0.0001f);
    }
}
