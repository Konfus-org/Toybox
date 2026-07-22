#include "tbx/assets/assets.h"
#include "tbx/reflection/type_registration.h"
#include "tbx/serialization/registration.h"
#include "tbx/runtime.h"
#include "tbx/events/events.h"
#include "tbx/serialization/json.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace tbx::tests
{
    /// @brief
    /// Purpose: A minimal asset for the test: no hand-written loader — a Format::DEFAULT
    /// serializer decodes generically through the reflection walker.
    struct ThingAsset : assets::Asset
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
        reflection::register_type<ThingAsset>("ThingAsset").field("answer", &ThingAsset::answer);
        serialization::register_serializer<ThingAsset>().format(serialization::Format::DEFAULT);
        auto toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        assets::set_root(runtime.assets, runtime.events, runtime.jobs, asset_root);
        auto unload_count = 0;
        runtime.events.asset_unloaded.subscribe(
            &unload_count,
            [&unload_count](const events::AssetUnloaded&) { ++unload_count; });
        const auto loaded =
            assets::load_now(runtime.assets, runtime.events, assets::Handle<ThingAsset>("thing.json"));
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        ASSERT_EQ(assets::get_loaded_count(runtime.assets), 1u);
        EXPECT_EQ(loaded->get().answer, 42);
        // A loaded asset knows its own handle.
        EXPECT_TRUE(loaded->get().id.is_valid());
        EXPECT_EQ(loaded->get().path, "thing.json");

        // Act: with a zero lifetime an unreferenced asset collects immediately.
        runtime.assets.idle_lifetime_seconds = 0.0f;
        assets::update(runtime.assets, runtime.events);
        events::update(runtime.events);

        // Assert
        EXPECT_EQ(assets::get_loaded_count(runtime.assets), 0u);
        EXPECT_EQ(unload_count, 1);

        // A fresh reference simply reloads it from disk.
        const auto reloaded =
            assets::load_now(runtime.assets, runtime.events, assets::Handle<ThingAsset>("thing.json"));
        ASSERT_TRUE(reloaded.has_value()) << reloaded.error();
        EXPECT_EQ(reloaded->get().answer, 42);
    }
}
