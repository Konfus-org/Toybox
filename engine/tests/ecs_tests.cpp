#include "tbx/serialization/serialization.h"
#include <gtest/gtest.h>
#include <map>
#include <thread>

namespace tbx::tests
{
    struct TestHealth
    {
        float hp = 100.0f;
        float armor = 0.0f;
    };

    static void register_ecs_test_blocks()
    {
        static bool g_registered = false;
        if (g_registered)
            return;
        g_registered = true;
        register_block<TestHealth>("TestHealth")
            .field("hp", &TestHealth::hp)
            .field("armor", &TestHealth::armor);
    }

    static KitResolver make_map_resolver(std::map<std::string, Json> kits)
    {
        return [kits = std::move(kits)](const std::string& reference) -> Result<Json>
        {
            const auto it = kits.find(reference);
            if (it == kits.end())
                return fail("unknown kit '{}'", reference);
            return it->second;
        };
    }

    /// @brief
    /// Purpose: Strips per-instantiation uuids so two saves of the same content compare equal.
    static Json normalize_kit(Json kit)
    {
        auto ordinal_by_uuid = std::map<std::string, int>();
        for (auto& toy : kit["toys"])
        {
            const auto uuid = toy.value("uuid", std::string());
            ordinal_by_uuid.emplace(uuid, static_cast<int>(ordinal_by_uuid.size()));
            toy["uuid"] = ordinal_by_uuid[uuid];
        }
        for (auto& toy : kit["toys"])
            if (toy.contains("parent"))
                toy["parent"] = ordinal_by_uuid[toy["parent"].get<std::string>()];
        return kit;
    }

    TEST(Sandbox, SpawnBuildsFluentToyWithBlocksAndStickers)
    {
        // Arrange
        register_ecs_test_blocks();
        auto jobs = Jobs();
        auto sandbox = Sandbox(jobs);

        // Act
        Toy grunt = sandbox.spawn("Grunt")
                        .with(Transform {.position = Vec3(1.0f, 2.0f, 3.0f)})
                        .with(TestHealth {.hp = 50.0f, .armor = 10.0f})
                        .sticker("enemy");

        // Assert
        EXPECT_TRUE(grunt.is_alive());
        EXPECT_EQ(grunt.get_name(), "Grunt");
        EXPECT_TRUE(grunt.has_block<TestHealth>());
        EXPECT_TRUE(grunt.has_sticker("enemy"));
        EXPECT_FALSE(grunt.has_sticker("pickup"));
        EXPECT_EQ(grunt.get_block<TestHealth>().hp, 50.0f);
        EXPECT_FALSE(grunt.get_uuid().is_nil());
    }

    TEST(Sandbox, DespawnKillsToyAndOrphansChildren)
    {
        // Arrange
        auto jobs = Jobs();
        auto sandbox = Sandbox(jobs);
        Toy parent = sandbox.spawn("Parent");
        Toy child = sandbox.spawn("Child");
        sandbox.set_parent(child, parent);

        // Act
        sandbox.despawn(parent);

        // Assert
        EXPECT_FALSE(parent.is_alive());
        EXPECT_TRUE(child.is_alive());
        EXPECT_FALSE(sandbox.get_parent(child).has_value());
    }

    TEST(Sandbox, FindsToysBySearchAndSticker)
    {
        // Arrange
        auto jobs = Jobs();
        auto sandbox = Sandbox(jobs);
        Toy grunt = sandbox.spawn("Grunt").sticker("enemy");
        sandbox.spawn("Crate");
        auto stickered = std::vector<std::string>();

        // Act
        const auto by_name = sandbox.find("Grunt");
        const auto by_uuid = sandbox.find(grunt.get_uuid());
        sandbox.for_each_sticker(
            "enemy",
            [&stickered](Toy toy) { stickered.push_back(toy.get_name()); });
        const auto missing = sandbox.find("Ghost");

        // Assert
        ASSERT_TRUE(by_name.has_value());
        ASSERT_TRUE(by_uuid.has_value());
        EXPECT_EQ(by_name->get_id(), grunt.get_id());
        EXPECT_EQ(by_uuid->get_id(), grunt.get_id());
        ASSERT_EQ(stickered.size(), 1u);
        EXPECT_EQ(stickered[0], "Grunt");
        EXPECT_FALSE(missing.has_value());
    }

    TEST(Sandbox, KitRoundTripPreservesContent)
    {
        // Arrange
        register_ecs_test_blocks();
        auto jobs = Jobs();
        auto source = Sandbox(jobs);
        Toy parent = source.spawn("Room")
                         .with(Transform {.position = Vec3(5.0f, 0.0f, 0.0f)})
                         .sticker("level");
        Toy child = source.spawn("Grunt").with(TestHealth {.hp = 33.0f, .armor = 1.0f});
        source.set_parent(child, parent);
        const Json kit = save(source, std::array {parent, child});

        // Act
        auto target = Sandbox(jobs);
        const auto loaded = load(target, kit);

        // Assert
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        EXPECT_EQ(target.get_toy_count(), 2u);
        const auto room = target.find("Room");
        const auto grunt = target.find("Grunt");
        ASSERT_TRUE(room.has_value());
        ASSERT_TRUE(grunt.has_value());
        EXPECT_TRUE(room->has_sticker("level"));
        EXPECT_EQ(Toy(*room).get_block<Transform>().position, Vec3(5.0f, 0.0f, 0.0f));
        EXPECT_EQ(Toy(*grunt).get_block<TestHealth>().hp, 33.0f);
        ASSERT_TRUE(target.get_parent(*grunt).has_value());
        EXPECT_EQ(target.get_parent(*grunt)->get_id(), room->get_id());
    }

    TEST(Sandbox, KitSaveIsStableAcrossRoundTrips)
    {
        // Arrange
        register_ecs_test_blocks();
        auto jobs = Jobs();
        auto source = Sandbox(jobs);
        Toy toy = source.spawn("Thing").with(TestHealth {.hp = 7.0f, .armor = 2.0f});
        const Json first = save(source, std::array {toy});

        // Act
        auto target = Sandbox(jobs);
        ASSERT_TRUE(load(target, first).has_value());
        const auto reloaded = target.find("Thing");
        ASSERT_TRUE(reloaded.has_value());
        const Json second = save(target, std::array {*reloaded});

        // Assert: identical content modulo per-instantiation uuids.
        EXPECT_EQ(normalize_kit(first), normalize_kit(second));
    }

    TEST(Sandbox, NestedKitsInstantiateRecursively)
    {
        // Arrange: prefab <- room <- level, three deep with position offsets.
        register_ecs_test_blocks();
        auto jobs = Jobs();
        auto author = Sandbox(jobs);
        const Json prefab = save(author, std::array {author.spawn("Pickup")});
        auto room = Json {
            {"toys", Json::array()},
            {"kits", Json::array({Json {{"reference", "prefab"},
                                        {"position", {1.0f, 0.0f, 0.0f}}}})}};
        auto level = Json {
            {"toys", Json::array()},
            {"kits", Json::array({Json {{"reference", "room"},
                                        {"position", {10.0f, 0.0f, 0.0f}}}})}};
        const auto resolver =
            make_map_resolver({{"prefab", prefab}, {"room", room}, {"level", level}});

        // Act
        auto sandbox = Sandbox(jobs);
        const auto loaded = load(sandbox, level, Vec3(100.0f, 0.0f, 0.0f), resolver);

        // Assert: offsets compose 100 + 10 + 1.
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        const auto pickup = sandbox.find("Pickup");
        ASSERT_TRUE(pickup.has_value());
        EXPECT_EQ(Toy(*pickup).get_block<Transform>().position, Vec3(111.0f, 0.0f, 0.0f));
    }

    TEST(Sandbox, KitReferenceCycleFailsAndRollsBack)
    {
        // Arrange: a references b references a.
        auto jobs = Jobs();
        auto a = Json {
            {"toys",
             Json::array({Json {{"uuid", "00"}, {"name", "InsideA"}, {"blocks", Json::array()}}})},
            {"kits", Json::array({Json {{"reference", "b"}}})}};
        auto b = Json {
            {"toys", Json::array()},
            {"kits", Json::array({Json {{"reference", "a"}}})}};
        const auto resolver = make_map_resolver({{"a", a}, {"b", b}});
        auto sandbox = Sandbox(jobs);

        // Act
        const auto loaded = load(sandbox, a, Vec3(0.0f), resolver);

        // Assert: error mentions the cycle and no partial toys survive.
        ASSERT_FALSE(loaded.has_value());
        EXPECT_NE(loaded.error().find("cycle"), std::string::npos);
        EXPECT_EQ(sandbox.get_toy_count(), 0u);
    }

    TEST(Sandbox, KitReferenceWithoutResolverFails)
    {
        // Arrange
        auto jobs = Jobs();
        auto kit = Json {
            {"toys", Json::array()},
            {"kits", Json::array({Json {{"reference", "anything"}}})}};
        auto sandbox = Sandbox(jobs);

        // Act
        const auto loaded = load(sandbox, kit);

        // Assert
        EXPECT_FALSE(loaded.has_value());
    }

    TEST(Sandbox, UnknownBlockTypeIsSkippedNotFatal)
    {
        // Arrange
        auto jobs = Jobs();
        auto kit = Json {
            {"toys",
             Json::array({Json {
                 {"uuid", "00"},
                 {"name", "Survivor"},
                 {"blocks",
                  Json::array({Json {{"__type", "EditorOnlyWidget"}, {"whatever", 1}}})}}})}};
        auto sandbox = Sandbox(jobs);

        // Act
        const auto loaded = load(sandbox, kit);

        // Assert
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        EXPECT_TRUE(sandbox.find("Survivor").has_value());
    }

    TEST(Sandbox, StreamedKitLoadsAndUnloadsWithHysteresis)
    {
        // Arrange: a kit whose bounds sit at the origin with radius 0.
        register_ecs_test_blocks();
        auto jobs = Jobs();
        auto author = Sandbox(jobs);
        const Json body = save(author, std::array {author.spawn("RoomToy")});
        auto layout = Json {
            {"kits",
             Json::array({Json {
                 {"reference", "room"},
                 {"mode", "streamed"},
                 {"position", {0.0f, 0.0f, 0.0f}}}})}};
        auto sandbox = Sandbox(jobs);
        ASSERT_TRUE(sandbox.open({.kits = layout, .resolver = make_map_resolver({{"room", body}})}).has_value());
        EXPECT_EQ(sandbox.get_toy_count(), 0u); // streamed entries do not preload

        // Act: focus inside the load band → the kit streams in (async).
        sandbox.stream(Vec3(1.0f, 0.0f, 0.0f));
        for (int i = 0; i < 500 && sandbox.get_toy_count() == 0; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            jobs.drain_main();
        }
        const size loaded_count = sandbox.get_toy_count();

        // Focus just outside the load band but inside the unload band → stays loaded.
        sandbox.stream(Vec3(10.0f, 0.0f, 0.0f));
        jobs.drain_main();
        const size hysteresis_count = sandbox.get_toy_count();

        // Focus beyond the unload band → unloads.
        sandbox.stream(Vec3(100.0f, 0.0f, 0.0f));
        jobs.drain_main();

        // Assert
        EXPECT_EQ(loaded_count, 1u);
        EXPECT_EQ(hysteresis_count, 1u);
        EXPECT_EQ(sandbox.get_toy_count(), 0u);
    }

    TEST(Sandbox, LayoutAlwaysKitsLoadImmediately)
    {
        // Arrange
        auto jobs = Jobs();
        auto author = Sandbox(jobs);
        const Json body = save(author, std::array {author.spawn("Skybox")});
        auto layout = Json {
            {"kits", Json::array({Json {{"reference", "sky"}, {"mode", "always"}}})}};
        auto sandbox = Sandbox(jobs);

        // Act
        const auto result = sandbox.open({.kits = layout, .resolver = make_map_resolver({{"sky", body}})});

        // Assert
        ASSERT_TRUE(result.has_value()) << result.error();
        EXPECT_TRUE(sandbox.find("Skybox").has_value());
    }

    TEST(Sandbox, DisabledStatePersistsThroughKits)
    {
        // Arrange
        auto jobs = Jobs();
        auto source = Sandbox(jobs);
        Toy toy = source.spawn("Lamp");
        toy.set_enabled(false);
        const Json kit = save(source, std::array {toy});

        // Act
        auto target = Sandbox(jobs);
        ASSERT_TRUE(load(target, kit).has_value());

        // Assert
        const auto reloaded = target.find("Lamp");
        ASSERT_TRUE(reloaded.has_value());
        EXPECT_FALSE(Toy(*reloaded).is_enabled());
        EXPECT_TRUE(source.spawn("Fresh").is_enabled()); // default stays on
    }

    TEST(Sandbox, CloseEmptiesTheSandboxForReopening)
    {
        // Arrange
        auto jobs = Jobs();
        auto author = Sandbox(jobs);
        const Json body = save(author, std::array {author.spawn("Skybox")});
        auto layout = Json {
            {"kits", Json::array({Json {{"reference", "sky"}, {"mode", "always"}}})}};
        auto sandbox = Sandbox(jobs);
        ASSERT_TRUE(
            sandbox.open({.kits = layout, .resolver = make_map_resolver({{"sky", body}})})
                .has_value());
        ASSERT_EQ(sandbox.get_toy_count(), 1u);

        // Act
        sandbox.close();

        // Assert: empty, and a fresh open works again.
        EXPECT_EQ(sandbox.get_toy_count(), 0u);
        EXPECT_TRUE(
            sandbox.open({.kits = layout, .resolver = make_map_resolver({{"sky", body}})})
                .has_value());
        EXPECT_EQ(sandbox.get_toy_count(), 1u);
    }

    TEST(Sandbox, WorldMatrixComposesParentChain)
    {
        // Arrange
        auto jobs = Jobs();
        auto sandbox = Sandbox(jobs);
        Toy parent = sandbox.spawn("Parent").with(Transform {.position = Vec3(10.0f, 0.0f, 0.0f)});
        Toy child = sandbox.spawn("Child").with(Transform {.position = Vec3(0.0f, 5.0f, 0.0f)});
        sandbox.set_parent(child, parent);

        // Act
        const Mat4 world = sandbox.get_world_matrix(child);
        const Vec4 origin = world * Vec4(0.0f, 0.0f, 0.0f, 1.0f);

        // Assert
        EXPECT_NEAR(origin.x, 10.0f, 0.0001f);
        EXPECT_NEAR(origin.y, 5.0f, 0.0001f);
    }
}
