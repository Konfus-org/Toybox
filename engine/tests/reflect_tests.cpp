#include "tbx/gfx/shader_source.h"
#include "tbx/serialization/json_walker.h"
#include "tbx/reflection/type_info.h"
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

    static const reflection::TypeInfo& register_test_types()
    {
        reflection::register_type<TestStats>("TestStats")
            .field("wins", &TestStats::wins)
            .field("rating", &TestStats::rating);
        reflection::register_type<TestPlayer>("TestPlayer")
            .version(
                2,
                [](serialization::Json& data, uint32)
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
        return reflection::get_type_registry().find("TestPlayer")->get();
    }

    struct TestChain
    {
        std::vector<AssetHandle<ShaderSource>> shaders = {};
    };

    TEST(Reflect, RoundTripsAssetHandleLists)
    {
        // Arrange
        reflection::register_type<TestChain>("TestChain").field("shaders", &TestChain::shaders);
        const reflection::TypeInfo& type = reflection::get_type_registry().find("TestChain")->get();
        auto original = TestChain {};
        original.shaders.push_back(AssetHandle<ShaderSource>(Uuid::generate()));
        original.shaders.push_back(AssetHandle<ShaderSource>(Uuid::generate()));

        // Act
        const serialization::Json data = serialization::json_write(type, original);
        auto loaded = TestChain {};
        const auto result = serialization::json_read(type, loaded, data);

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
        const reflection::TypeInfo& type = register_test_types();
        auto original = TestPlayer {};
        original.hp = 42.5f;
        original.title = "boss";
        original.position = Vec3(1.0f, 2.0f, 3.0f);
        original.mode = TestMode::ANGRY;
        original.stats = TestStats {.wins = 9, .rating = 4.5f};
        original.id = Uuid::generate();

        // Act
        const serialization::Json data = serialization::json_write(type, original);
        auto loaded = TestPlayer {};
        const auto result = serialization::json_read(type, loaded, data);

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
        const reflection::TypeInfo& type = register_test_types();
        auto target = TestPlayer {};

        // Act
        const auto result = serialization::json_read(type, target, serialization::Json::array());

        // Assert
        EXPECT_FALSE(result.has_value());
    }

    TEST(Reflect, MigrateRunsForOlderVersions)
    {
        // Arrange
        const reflection::TypeInfo& type = register_test_types();
        auto old_data = serialization::Json::object();
        old_data["type"] = "TestPlayer";
        old_data["version"] = 1;
        old_data["health"] = 77.0f; // the v1 field name

        // Act
        auto loaded = TestPlayer {};
        const auto result = serialization::json_read(type, loaded, old_data);

        // Assert
        ASSERT_TRUE(result.has_value()) << result.error();
        EXPECT_EQ(loaded.hp, 77.0f);
    }

    TEST(Reflect, MigrateDoesNotRunForCurrentVersion)
    {
        // Arrange
        const reflection::TypeInfo& type = register_test_types();
        auto current = serialization::Json::object();
        current["version"] = 2;
        current["health"] = 5.0f; // stale name would only be fixed by migrate
        current["hp"] = 50.0f;

        // Act
        auto loaded = TestPlayer {};
        const auto result = serialization::json_read(type, loaded, current);

        // Assert
        ASSERT_TRUE(result.has_value()) << result.error();
        EXPECT_EQ(loaded.hp, 50.0f);
    }

    TEST(Reflect, MissingFieldsKeepDefaults)
    {
        // Arrange
        const reflection::TypeInfo& type = register_test_types();
        auto sparse = serialization::Json::object();
        sparse["version"] = 2;
        sparse["hp"] = 12.0f;

        // Act
        auto loaded = TestPlayer {};
        const auto result = serialization::json_read(type, loaded, sparse);

        // Assert
        ASSERT_TRUE(result.has_value()) << result.error();
        EXPECT_EQ(loaded.hp, 12.0f);
        EXPECT_EQ(loaded.title, "rookie"); // untouched default
    }

    TEST(Reflect, WrongShapedFieldReportsError)
    {
        // Arrange
        const reflection::TypeInfo& type = register_test_types();
        auto bad = serialization::Json::object();
        bad["version"] = 2;
        bad["hp"] = "not a number";

        // Act
        auto loaded = TestPlayer {};
        const auto result = serialization::json_read(type, loaded, bad);

        // Assert
        ASSERT_FALSE(result.has_value());
        EXPECT_NE(result.error().find("hp"), std::string::npos);
    }

    TEST(Reflect, RegistryFindsTypesByNameAndHash)
    {
        // Arrange
        register_test_types();

        // Act
        const auto by_name = reflection::get_type_registry().find("TestPlayer");
        const auto by_hash = reflection::get_type_registry().find(hash("TestPlayer"));

        // Assert
        ASSERT_TRUE(by_name.has_value());
        ASSERT_TRUE(by_hash.has_value());
        EXPECT_EQ(&by_name->get(), &by_hash->get());
        EXPECT_EQ(by_name->get().version, 2u);
    }

    TEST(Reflect, RegistryFindMissesUnregisteredNames)
    {
        // Arrange / Act
        const auto missing = reflection::get_type_registry().find("NeverRegistered");

        // Assert
        EXPECT_FALSE(missing.has_value());
    }
}
