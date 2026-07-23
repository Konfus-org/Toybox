#include "reflection/reflection_internal.h"
#include "tbx/gfx/shader_source.h"
#include "tbx/serialization/json.h"
#include "tbx/reflection/reflection.h"
#include <gtest/gtest.h>

namespace tbx
{
    enum class TestMode : uint8
    {
        IDLE = 0,
        ANGRY = 3
    };

    struct TestStats
    {
        int32 wins = 0;
        float rating = 1.0f;
    };

    struct TestPlayer
    {
        float hp = 100.0f;
        std::string title = "rookie";
        Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
        TestMode mode = TestMode::IDLE;
        TestStats stats = {};
        Uuid id = {};
    };

    static const TypeInfo& register_test_types()
    {
        register_type<TestStats>("TestStats")
            .field("wins", &TestStats::wins)
            .field("rating", &TestStats::rating);
        register_type<TestPlayer>("TestPlayer")
            .version(
                2,
                [](Json& data, uint32)
                {
                    // v1 stored "health"; v2 renamed it to "hp".
                    if (data.contains("health"))
                    {
                        data["hp"] = data["health"];
                        data.erase("health");
                    }
                })
            .field("hp", &TestPlayer::hp)
            .field("title", &TestPlayer::title)
            .field("position", &TestPlayer::position)
            .field("mode", &TestPlayer::mode)
            .field("stats", &TestPlayer::stats)
            .field("id", &TestPlayer::id);
        return get_type_registry().find("TestPlayer")->get();
    }

    struct TestChain
    {
        std::vector<AssetHandle<ShaderSource>> shaders = {};
    };

    TEST(Reflect, RoundTripsAssetHandleLists)
    {
        // Arrange
        register_type<TestChain>("TestChain").field("shaders", &TestChain::shaders);
        const TypeInfo& type = get_type_registry().find("TestChain")->get();
        auto original = TestChain {};
        original.shaders.push_back(AssetHandle<ShaderSource>(Uuid::generate()));
        original.shaders.push_back(AssetHandle<ShaderSource>(Uuid::generate()));

        // Act
        const Json data = json_write(type, original);
        auto loaded = TestChain {};
        const auto result = json_read(type, loaded, data);

        // Assert: an array of uuid strings, back to the same handles in the same order.
        ASSERT_TRUE(result.has_value()) << result.error();
        ASSERT_TRUE(data["shaders"].is_array());
        ASSERT_EQ(loaded.shaders.size(), original.shaders.size());
        EXPECT_EQ(loaded.shaders[0].id, original.shaders[0].id);
        EXPECT_EQ(loaded.shaders[1].id, original.shaders[1].id);
    }

    TEST(Reflect, RoundTripsAllFieldKinds)
    {
        // Arrange
        const TypeInfo& type = register_test_types();
        auto original = TestPlayer {};
        original.hp = 42.5f;
        original.title = "boss";
        original.position = Vec3(1.0f, 2.0f, 3.0f);
        original.mode = TestMode::ANGRY;
        original.stats = TestStats {.wins = 9, .rating = 4.5f};
        original.id = Uuid::generate();

        // Act
        const Json data = json_write(type, original);
        auto loaded = TestPlayer {};
        const auto result = json_read(type, loaded, data);

        // Assert
        ASSERT_TRUE(result.has_value()) << result.error();
        EXPECT_EQ(loaded.hp, original.hp);
        EXPECT_EQ(loaded.title, original.title);
        EXPECT_EQ(loaded.position, original.position);
        EXPECT_EQ(loaded.mode, original.mode);
        EXPECT_EQ(loaded.stats.wins, original.stats.wins);
        EXPECT_EQ(loaded.stats.rating, original.stats.rating);
        EXPECT_EQ(loaded.id, original.id);
    }

    TEST(Reflect, ReadRejectsNonObjectData)
    {
        // Arrange
        const TypeInfo& type = register_test_types();
        auto target = TestPlayer {};

        // Act
        const auto result = json_read(type, target, Json::array());

        // Assert
        EXPECT_FALSE(result.has_value());
    }

    TEST(Reflect, MigrateRunsForOlderVersions)
    {
        // Arrange
        const TypeInfo& type = register_test_types();
        auto old_data = Json::object();
        old_data["type"] = "TestPlayer";
        old_data["version"] = 1;
        old_data["health"] = 77.0f; // the v1 field name

        // Act
        auto loaded = TestPlayer {};
        const auto result = json_read(type, loaded, old_data);

        // Assert
        ASSERT_TRUE(result.has_value()) << result.error();
        EXPECT_EQ(loaded.hp, 77.0f);
    }

    TEST(Reflect, MigrateDoesNotRunForCurrentVersion)
    {
        // Arrange
        const TypeInfo& type = register_test_types();
        auto current = Json::object();
        current["version"] = 2;
        current["health"] = 5.0f; // stale name would only be fixed by migrate
        current["hp"] = 50.0f;

        // Act
        auto loaded = TestPlayer {};
        const auto result = json_read(type, loaded, current);

        // Assert
        ASSERT_TRUE(result.has_value()) << result.error();
        EXPECT_EQ(loaded.hp, 50.0f);
    }

    TEST(Reflect, MissingFieldsKeepDefaults)
    {
        // Arrange
        const TypeInfo& type = register_test_types();
        auto sparse = Json::object();
        sparse["version"] = 2;
        sparse["hp"] = 12.0f;

        // Act
        auto loaded = TestPlayer {};
        const auto result = json_read(type, loaded, sparse);

        // Assert
        ASSERT_TRUE(result.has_value()) << result.error();
        EXPECT_EQ(loaded.hp, 12.0f);
        EXPECT_EQ(loaded.title, "rookie"); // untouched default
    }

    TEST(Reflect, WrongShapedFieldReportsError)
    {
        // Arrange
        const TypeInfo& type = register_test_types();
        auto bad = Json::object();
        bad["version"] = 2;
        bad["hp"] = "not a number";

        // Act
        auto loaded = TestPlayer {};
        const auto result = json_read(type, loaded, bad);

        // Assert
        ASSERT_FALSE(result.has_value());
        EXPECT_NE(result.error().find("hp"), std::string::npos);
    }

    TEST(Reflect, RegistryFindsTypesByNameAndHash)
    {
        // Arrange
        register_test_types();

        // Act
        const auto by_name = get_type_registry().find("TestPlayer");
        const auto by_hash = get_type_registry().find(hash("TestPlayer"));

        // Assert
        ASSERT_TRUE(by_name.has_value());
        ASSERT_TRUE(by_hash.has_value());
        EXPECT_EQ(&by_name->get(), &by_hash->get());
        EXPECT_EQ(by_name->get().version, 2u);
    }

    TEST(Reflect, RegistryFindMissesUnregisteredNames)
    {
        // Arrange / Act
        const auto missing = get_type_registry().find("NeverRegistered");

        // Assert
        EXPECT_FALSE(missing.has_value());
    }

    TEST(Reflect, PurgeEmptiesTheRegistryAndReInitRestoresIt)
    {
        // Arrange: builtins present.
        internal::initialize_reflection();
        ASSERT_TRUE(is_reflection_ready());

        // Act + Assert: purge empties it, re-init refills the builtins.
        internal::purge_reflection_registry();
        EXPECT_FALSE(is_reflection_ready());
        EXPECT_TRUE(get_type_registry().get_all().empty());
        internal::initialize_reflection();
        EXPECT_TRUE(is_reflection_ready());
        EXPECT_TRUE(get_type_registry().find("App").has_value());
    }

    struct Pinged
    {
        int value = 0;
    };

    struct SignalBeeper : Block
    {
        Signal<Pinged> pinged;
    };

    TEST(Reflect, SignalMemberConnectsAndReceivesEmittedEvent)
    {
        // Arrange: a block with a reflected Signal member.
        register_type<SignalBeeper>("SignalBeeper").signal("pinged", &SignalBeeper::pinged);
        const auto info = describe_type<SignalBeeper>();
        ASSERT_TRUE(info.has_value());
        ASSERT_EQ(info->get().signals.size(), 1u);
        EXPECT_EQ(info->get().signals.front().name, "pinged");

        auto beeper = SignalBeeper();
        auto received = 0;
        info->get().signals.front().connect(
            reinterpret_cast<std::byte*>(&beeper),
            nullptr,
            [&received](const void* event) { received = static_cast<const Pinged*>(event)->value; });

        // Act: a queue-less component signal dispatches inline on emit.
        beeper.pinged.emit({.value = 42});

        // Assert
        EXPECT_EQ(received, 42);
    }

    TEST(Reflect, SignalWithNoConnectionsIsANoOpOnEmit)
    {
        // Arrange
        register_type<SignalBeeper>("SignalBeeper").signal("pinged", &SignalBeeper::pinged);
        auto beeper = SignalBeeper();

        // Act + Assert: emitting with nothing connected must not crash.
        beeper.pinged.emit({.value = 7});
        SUCCEED();
    }
}
