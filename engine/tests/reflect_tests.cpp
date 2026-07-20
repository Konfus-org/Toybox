#include "tbx/reflect/json_walker.h"
#include "tbx/reflect/type_info.h"
#include <gtest/gtest.h>

namespace tbx::tests
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
        old_data["__type"] = "TestPlayer";
        old_data["__version"] = 1;
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
        current["__version"] = 2;
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
        sparse["__version"] = 2;
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
        bad["__version"] = 2;
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
        const auto by_hash = get_type_registry().find(hash_name("TestPlayer"));

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
}
