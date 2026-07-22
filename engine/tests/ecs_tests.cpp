#include "tbx/assets/assets.h"
#include "tbx/reflection/type_registration.h"
#include "tbx/utils/hash.h"
#include "tbx/runtime.h"
#include "tbx/serialization/json.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>

namespace tbx::tests
{

    struct TestHealth : ecs::Block
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
        reflection::register_type<TestHealth>("TestHealth")
            .field("hp", &TestHealth::hp)
            .field("armor", &TestHealth::armor);
    }

    /// @brief
    /// Purpose: A temp asset root per test: kits written via write_kit() resolve through the
    /// same asset system a shipped game uses — no in-memory shortcuts.
    struct TestWorld
    {
        Runtime toybox = Runtime();
        RuntimeState& runtime = *toybox.state;
        std::filesystem::path root = {};

        TestWorld()
        {
            const auto* info = testing::UnitTest::GetInstance()->current_test_info();
            root = std::filesystem::temp_directory_path() / "tbx_ecs_tests" / info->name();
            std::filesystem::remove_all(root);
            std::filesystem::create_directories(root);
            assets::set_root(runtime.assets, runtime.events, runtime.jobs, root);
        }
    };

    /// @brief
    /// Purpose: Writes one kit body into the world's asset root.
    static void write_kit(const TestWorld& world, const std::string& name, const serialization::Json& body)
    {
        auto file = std::ofstream(world.root / name);
        file << body.dump();
    }

    /// @brief
    /// Purpose: Strips per-instantiation uuids so two saves of the same content compare equal.
    static serialization::Json normalize_kit(serialization::Json kit)
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
        auto sandbox = ecs::Sandbox();

        // Act
        ecs::Toy grunt = sandbox.spawn("Grunt")
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
        EXPECT_TRUE(grunt.get_uuid().is_valid());
    }

    TEST(Sandbox, DespawnKillsToyAndOrphansChildren)
    {
        // Arrange
        auto sandbox = ecs::Sandbox();
        ecs::Toy parent = sandbox.spawn("Parent");
        ecs::Toy child = sandbox.spawn("Child");
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
        auto sandbox = ecs::Sandbox();
        ecs::Toy grunt = sandbox.spawn("Grunt").sticker("enemy");
        sandbox.spawn("Crate");
        auto stickered = std::vector<std::string>();

        // Act
        const auto by_name = sandbox.find("Grunt");
        const auto by_uuid = sandbox.find(grunt.get_uuid());
        sandbox.for_each_sticker(
            "enemy",
            [&stickered](ecs::Toy toy) { stickered.push_back(toy.get_name()); });
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
        auto source = ecs::Sandbox();
        ecs::Toy parent = source.spawn("Room")
                         .with(Transform {.position = Vec3(5.0f, 0.0f, 0.0f)})
                         .sticker("level");
        ecs::Toy child = source.spawn("Grunt").with(TestHealth {.hp = 33.0f, .armor = 1.0f});
        source.set_parent(child, parent);
        const ecs::Kit kit = save(source, std::array {parent, child});

        // Act
        auto runtime = Runtime();
        auto target = ecs::Sandbox();
        const auto loaded = load(target, runtime.state->assets, runtime.state->events, kit);

        // Assert
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        EXPECT_EQ(target.get_toy_count(), 2u);
        const auto room = target.find("Room");
        const auto grunt = target.find("Grunt");
        ASSERT_TRUE(room.has_value());
        ASSERT_TRUE(grunt.has_value());
        EXPECT_TRUE(room->has_sticker("level"));
        EXPECT_EQ(ecs::Toy(*room).get_block<Transform>().position, Vec3(5.0f, 0.0f, 0.0f));
        EXPECT_EQ(ecs::Toy(*grunt).get_block<TestHealth>().hp, 33.0f);
        ASSERT_TRUE(target.get_parent(*grunt).has_value());
        EXPECT_EQ(target.get_parent(*grunt)->get_id(), room->get_id());
    }

    TEST(Sandbox, KitSaveIsStableAcrossRoundTrips)
    {
        // Arrange
        register_ecs_test_blocks();
        auto source = ecs::Sandbox();
        ecs::Toy toy = source.spawn("Thing").with(TestHealth {.hp = 7.0f, .armor = 2.0f});
        const ecs::Kit first = save(source, std::array {toy});

        // Act
        auto runtime = Runtime();
        auto target = ecs::Sandbox();
        ASSERT_TRUE(load(target, runtime.state->assets, runtime.state->events, first).has_value());
        const auto reloaded = target.find("Thing");
        ASSERT_TRUE(reloaded.has_value());
        const ecs::Kit second = save(target, std::array {*reloaded});

        // Assert: identical content modulo per-instantiation uuids.
        EXPECT_EQ(normalize_kit(to_json(first)), normalize_kit(to_json(second)));
    }

    TEST(Sandbox, NestedKitsInstantiateRecursively)
    {
        // Arrange: prefab <- room <- level, three deep with position offsets — the nested
        // references are ordinary kit assets on disk.
        register_ecs_test_blocks();
        auto world = TestWorld();
        auto author = ecs::Sandbox();
        write_kit(world, "prefab.kit", to_json(save(author, std::array {author.spawn("Pickup")})));
        write_kit(
            world,
            "room.kit",
            serialization::Json {
                {"toys", serialization::Json::array()},
                {"kits",
                 serialization::Json::array(
                     {serialization::Json {{"reference", "prefab.kit"}, {"position", {1.0f, 0.0f, 0.0f}}}})}});
        write_kit(
            world,
            "level.kit",
            serialization::Json {
                {"toys", serialization::Json::array()},
                {"kits",
                 serialization::Json::array(
                     {serialization::Json {{"reference", "room.kit"}, {"position", {10.0f, 0.0f, 0.0f}}}})}});

        // Act
        auto sandbox = ecs::Sandbox();
        const auto loaded =
            sandbox.spawn(world.runtime.assets, world.runtime.events, assets::AssetHandle<ecs::Kit>("level.kit"), Vec3(100.0f, 0.0f, 0.0f));

        // Assert: offsets compose 100 + 10 + 1.
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        const auto pickup = sandbox.find("Pickup");
        ASSERT_TRUE(pickup.has_value());
        EXPECT_EQ(ecs::Toy(*pickup).get_block<Transform>().position, Vec3(111.0f, 0.0f, 0.0f));
    }

    TEST(Sandbox, KitReferenceCycleFailsAndRollsBack)
    {
        // Arrange: a references b references a.
        auto world = TestWorld();
        write_kit(
            world,
            "a.kit",
            serialization::Json {
                {"toys",
                 serialization::Json::array(
                     {serialization::Json {{"uuid", "00"}, {"name", "InsideA"}, {"blocks", serialization::Json::array()}}})},
                {"kits", serialization::Json::array({serialization::Json {{"reference", "b.kit"}}})}});
        write_kit(
            world,
            "b.kit",
            serialization::Json {
                {"toys", serialization::Json::array()},
                {"kits", serialization::Json::array({serialization::Json {{"reference", "a.kit"}}})}});
        auto sandbox = ecs::Sandbox();

        // Act
        const auto loaded = sandbox.spawn(world.runtime.assets, world.runtime.events, assets::AssetHandle<ecs::Kit>("a.kit"));

        // Assert: error mentions the cycle and no partial toys survive.
        ASSERT_FALSE(loaded.has_value());
        EXPECT_NE(loaded.error().find("cycle"), std::string::npos);
        EXPECT_EQ(sandbox.get_toy_count(), 0u);
    }

    TEST(Sandbox, KitReferenceToMissingAssetFails)
    {
        // Arrange: the kit references an asset that does not exist.
        auto world = TestWorld();
        write_kit(
            world,
            "broken.kit",
            serialization::Json {
                {"toys", serialization::Json::array()},
                {"kits", serialization::Json::array({serialization::Json {{"reference", "missing.kit"}}})}});
        auto sandbox = ecs::Sandbox();

        // Act
        const auto loaded = sandbox.spawn(world.runtime.assets, world.runtime.events, assets::AssetHandle<ecs::Kit>("broken.kit"));

        // Assert
        EXPECT_FALSE(loaded.has_value());
        EXPECT_EQ(sandbox.get_toy_count(), 0u);
    }

    TEST(Sandbox, UnknownBlockTypeIsSkippedNotFatal)
    {
        // Arrange
        auto runtime = Runtime();
        auto sandbox = ecs::Sandbox();
        auto unknown_kit = ecs::Kit();
        auto survivor = ecs::KitToy();
        survivor.name = "Survivor";
        survivor.blocks.push_back(ecs::KitBlock {.type = hash("EditorOnlyWidget"), .value = {}});
        unknown_kit.toys.push_back(std::move(survivor));

        // Act
        const auto loaded = load(sandbox, runtime.state->assets, runtime.state->events, unknown_kit);

        // Assert
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        EXPECT_TRUE(sandbox.find("Survivor").has_value());
    }

    TEST(Sandbox, StreamedKitLoadsAndUnloadsWithHysteresis)
    {
        // Arrange: a kit whose bounds sit at the origin with radius 0.
        register_ecs_test_blocks();
        auto world = TestWorld();
        auto author = ecs::Sandbox();
        write_kit(world, "room.kit", to_json(save(author, std::array {author.spawn("RoomToy")})));
        const auto box = ecs::Box {
            .kits = {
                ecs::BoxEntry {.kit = assets::AssetHandle<ecs::Kit>("room.kit"), .mode = ecs::KitMode::STREAMED}}};
        auto sandbox = ecs::Sandbox();
        ASSERT_TRUE(sandbox.open(world.runtime.assets, world.runtime.events, box).has_value());
        EXPECT_EQ(sandbox.get_toy_count(), 0u); // streamed entries do not preload

        // Act: focus inside the load band — the kit streams in (async).
        sandbox.stream(world.runtime.assets, world.runtime.events, world.runtime.jobs, Vec3(1.0f, 0.0f, 0.0f));
        for (int i = 0; i < 500 && sandbox.get_toy_count() == 0; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            jobs::update(world.runtime.jobs);
        }
        const size loaded_count = sandbox.get_toy_count();

        // Focus just outside the load band but inside the unload band — stays loaded.
        sandbox.stream(world.runtime.assets, world.runtime.events, world.runtime.jobs, Vec3(10.0f, 0.0f, 0.0f));
        jobs::update(world.runtime.jobs);
        const size hysteresis_count = sandbox.get_toy_count();

        // Focus beyond the unload band — unloads.
        sandbox.stream(world.runtime.assets, world.runtime.events, world.runtime.jobs, Vec3(100.0f, 0.0f, 0.0f));
        jobs::update(world.runtime.jobs);

        // Assert
        EXPECT_EQ(loaded_count, 1u);
        EXPECT_EQ(hysteresis_count, 1u);
        EXPECT_EQ(sandbox.get_toy_count(), 0u);
    }

    TEST(Sandbox, BoxAlwaysKitsLoadImmediately)
    {
        // Arrange
        auto world = TestWorld();
        auto author = ecs::Sandbox();
        write_kit(world, "sky.kit", to_json(save(author, std::array {author.spawn("Skybox")})));
        const auto box = ecs::Box {.kits = {ecs::BoxEntry {.kit = assets::AssetHandle<ecs::Kit>("sky.kit")}}};
        auto sandbox = ecs::Sandbox();

        // Act
        const auto result = sandbox.open(world.runtime.assets, world.runtime.events, box);

        // Assert
        ASSERT_TRUE(result.has_value()) << result.error();
        EXPECT_TRUE(sandbox.find("Skybox").has_value());
    }

    TEST(Sandbox, DisabledStatePersistsThroughKits)
    {
        // Arrange
        auto source = ecs::Sandbox();
        ecs::Toy toy = source.spawn("Lamp");
        toy.set_enabled(false);
        const ecs::Kit kit = save(source, std::array {toy});

        // Act
        auto runtime = Runtime();
        auto target = ecs::Sandbox();
        ASSERT_TRUE(load(target, runtime.state->assets, runtime.state->events, kit).has_value());

        // Assert
        const auto reloaded = target.find("Lamp");
        ASSERT_TRUE(reloaded.has_value());
        EXPECT_FALSE(ecs::Toy(*reloaded).is_enabled());
        EXPECT_TRUE(source.spawn("Fresh").is_enabled()); // default stays on
    }

    TEST(Sandbox, CloseEmptiesTheSandboxForReopening)
    {
        // Arrange
        auto world = TestWorld();
        auto author = ecs::Sandbox();
        write_kit(world, "sky.kit", to_json(save(author, std::array {author.spawn("Skybox")})));
        const auto box = ecs::Box {.kits = {ecs::BoxEntry {.kit = assets::AssetHandle<ecs::Kit>("sky.kit")}}};
        auto sandbox = ecs::Sandbox();
        ASSERT_TRUE(sandbox.open(world.runtime.assets, world.runtime.events, box).has_value());
        ASSERT_EQ(sandbox.get_toy_count(), 1u);

        // Act
        sandbox.close();

        // Assert: empty, and a fresh open works again.
        EXPECT_EQ(sandbox.get_toy_count(), 0u);
        EXPECT_TRUE(sandbox.open(world.runtime.assets, world.runtime.events, box).has_value());
        EXPECT_EQ(sandbox.get_toy_count(), 1u);
    }

    TEST(Sandbox, WorldMatrixComposesParentChain)
    {
        // Arrange
        auto sandbox = ecs::Sandbox();
        ecs::Toy parent = sandbox.spawn("Parent").with(Transform {.position = Vec3(10.0f, 0.0f, 0.0f)});
        ecs::Toy child = sandbox.spawn("Child").with(Transform {.position = Vec3(0.0f, 5.0f, 0.0f)});
        sandbox.set_parent(child, parent);

        // Act
        const Mat4 world = sandbox.get_world_matrix(child);
        const Vec4 origin = world * Vec4(0.0f, 0.0f, 0.0f, 1.0f);

        // Assert
        EXPECT_NEAR(origin.x, 10.0f, 0.0001f);
        EXPECT_NEAR(origin.y, 5.0f, 0.0001f);
    }
}
