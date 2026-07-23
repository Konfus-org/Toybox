#include "tbx/assets/assets.h"
#include "tbx/reflection/type_registration.h"
#include "tbx/serialization/registration.h"
#include "tbx/runtime.h"
#include "tbx/events/events.h"
#include "tbx/serialization/json.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace tbx
{
    /// @brief
    /// Purpose: A minimal asset for the test: no hand-written loader — a Format::DEFAULT
    /// serializer decodes generically through the reflection walker.
    struct ThingAsset : Asset
    {
        int answer = 0;
    };

    /// @brief
    /// Purpose: A second asset type for the purge test — a distinct type so its one-shot
    /// registration never collides with ThingAsset's (re-registering would duplicate fields).
    struct PurgeThing : Asset
    {
        int answer = 0;
    };

    TEST(Assets, IdleAssetsAreCollectedAndAnnounced)
    {
        // Arrange: a real file in a temp root, loaded once, with a zero idle lifetime.
        const auto asset_root = std::filesystem::temp_directory_path() / "tbx_assets_test";
        std::filesystem::create_directories(asset_root);
        {
            auto file = std::ofstream(asset_root / "thing.json");
            file << "{\"answer\": 42}";
        }
        register_type<ThingAsset>("ThingAsset").field("answer", &ThingAsset::answer);
        register_serializer<ThingAsset>().format(SerializerFormat::DEFAULT);
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        set_asset_root(runtime.assets, runtime.events, runtime.jobs, asset_root);
        auto unload_count = 0;
        runtime.events.signal<AssetUnloaded>().subscribe(
            &unload_count,
            [&unload_count](const AssetUnloaded&) { ++unload_count; });
        const auto loaded =
            load_asset_now(runtime.assets, runtime.events, AssetHandle<ThingAsset>("thing.json"));
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        ASSERT_EQ(get_loaded_asset_count(runtime.assets), 1u);
        EXPECT_EQ(loaded->get().answer, 42);
        // A loaded asset knows its own handle.
        EXPECT_TRUE(loaded->get().id.is_valid());
        EXPECT_EQ(loaded->get().path, "thing.json");

        // Act: with a zero lifetime an unreferenced asset collects immediately.
        runtime.assets.idle_lifetime_seconds = 0.0f;
        update_assets(runtime.assets, runtime.events);
        update_events(runtime.events);

        // Assert
        EXPECT_EQ(get_loaded_asset_count(runtime.assets), 0u);
        EXPECT_EQ(unload_count, 1);

        // A fresh reference simply reloads it from disk.
        const auto reloaded =
            load_asset_now(runtime.assets, runtime.events, AssetHandle<ThingAsset>("thing.json"));
        ASSERT_TRUE(reloaded.has_value()) << reloaded.error();
        EXPECT_EQ(reloaded->get().answer, 42);
    }

    TEST(Assets, PurgeDropsResidentAssetsButStaysReady)
    {
        // Arrange: a real file in a temp root, loaded once via initialize_assets.
        const auto asset_root = std::filesystem::temp_directory_path() / "tbx_assets_purge_test";
        std::filesystem::create_directories(asset_root);
        {
            auto file = std::ofstream(asset_root / "purge_thing.json");
            file << "{\"answer\": 7}";
        }
        register_type<PurgeThing>("PurgeThing").field("answer", &PurgeThing::answer);
        register_serializer<PurgeThing>().format(SerializerFormat::DEFAULT);
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;

        // is_assets_ready flips only once the subsystem is stood up.
        EXPECT_FALSE(is_assets_ready(runtime.assets));
        initialize_assets(runtime.assets, runtime.events, runtime.jobs, asset_root);
        EXPECT_TRUE(is_assets_ready(runtime.assets));

        auto unload_count = 0;
        runtime.events.signal<AssetUnloaded>().subscribe(
            &unload_count,
            [&unload_count](const AssetUnloaded&) { ++unload_count; });
        const auto loaded = load_asset_now(
            runtime.assets, runtime.events, AssetHandle<PurgeThing>("purge_thing.json"));
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        ASSERT_EQ(get_loaded_asset_count(runtime.assets), 1u);

        // Act: purge drops every resident asset, announcing each.
        purge_assets(runtime.assets, runtime.events);
        update_events(runtime.events);

        // Assert: memory freed, one announcement, but the subsystem stays initialized.
        EXPECT_EQ(get_loaded_asset_count(runtime.assets), 0u);
        EXPECT_EQ(unload_count, 1);
        EXPECT_TRUE(is_assets_ready(runtime.assets));
    }
}
