#include "tbx/assets/assets.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace tbx::tests
{
    TEST(Assets, IdleAssetsAreCollectedAndAnnounced)
    {
        // Arrange: a real file in a temp root, loaded once, with a zero idle lifetime.
        const auto asset_root = std::filesystem::temp_directory_path() / "tbx_assets_test";
        std::filesystem::create_directories(asset_root);
        {
            auto file = std::ofstream(asset_root / "thing.json");
            file << "{\"answer\": 42}";
        }
        auto jobs = Jobs();
        auto events = Events();
        auto assets = Assets(jobs, events);
        assets.set_root(asset_root);
        auto unload_count = 0;
        events.asset_unloaded.subscribe(
            &unload_count,
            [&unload_count](const AssetUnloaded&) { ++unload_count; });
        const auto loaded = assets.load_now(AssetHandle<Json>("thing.json"));
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        ASSERT_EQ(assets.get_loaded_count(), 1u);

        // Act: with a zero lifetime an unreferenced asset collects immediately.
        assets.set_idle_lifetime(0.0f);
        assets.collect_garbage();
        events.drain();

        // Assert
        EXPECT_EQ(assets.get_loaded_count(), 0u);
        EXPECT_EQ(unload_count, 1);

        // A fresh reference simply reloads it from disk.
        const auto reloaded = assets.load_now(AssetHandle<Json>("thing.json"));
        ASSERT_TRUE(reloaded.has_value()) << reloaded.error();
        EXPECT_EQ(reloaded->get()["answer"].get<int>(), 42);
    }
}
