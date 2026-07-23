#include "tbx/assets/assets.h"
#include "tbx/files/files.h"
#include "tbx/utils/hash.h"
#include "tbx/math/frustum.h"
#include "tbx/reflection/reflection.h"
#include "tbx/reflection/type_registration.h"
#include "tbx/runtime.h"
#include "tbx/serialization/json.h"
#include "tbx/serialization/read_write.h"
#include "tbx/serialization/serializers.h"
#include <filesystem>
#include <gtest/gtest.h>
#include <thread>

namespace tbx
{
    // Kit add now lives on the Sandbox, which reads the asset system through wired-in refs
    // (set at boot). Tests build fresh sandboxes, so this wires them before instantiating a kit.
    template <typename TKit>
    static Result<Toy> add_kit(
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        const TKit& kit,
        const Vec3& position = Vec3(0.0f, 0.0f, 0.0f))
    {
        sandbox.assets = &assets;
        sandbox.events = &events;
        return sandbox.add(kit, position);
    }

    struct TestHealth : Block
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
        register_type<TestHealth>("TestHealth")
            .field("hp", &TestHealth::hp)
            .field("armor", &TestHealth::armor);
    }

    /// @brief
    /// Purpose: A temp asset root per test: kits written via write resolve
    /// through the same asset system a shipped game uses — no in-memory shortcuts.
    struct TestWorld
    {
        Runtime toybox = Runtime();
        internal::RuntimeState& runtime = *toybox.state;
        std::filesystem::path root = {};

        TestWorld()
        {
            // Stand reflection + serializers up before anything touches the sandbox — the world
            // is plain data now and no longer self-registers. set_asset_root (unlike
            // initialize_assets) does not do this, so do it explicitly.
            initialize_reflection();
            register_builtin_serializers();
            const auto* info = testing::UnitTest::GetInstance()->current_test_info();
            root = std::filesystem::temp_directory_path() / "tbx_ecs_tests" / info->name();
            std::filesystem::remove_all(root);
            std::filesystem::create_directories(root);
            set_asset_root(runtime.assets, runtime.events, runtime.jobs, root);
        }

        /// @brief
        /// Purpose: One stream tick with no cameras — flushes a pending open() without
        /// making any streaming decisions.
        void flush_open(Sandbox& sandbox)
        {
            stream(sandbox, runtime.assets, runtime.events, runtime.jobs, {});
        }
    };

    /// @brief
    /// Purpose: The frustum of a camera at eye looking at a point — what a real camera
    /// would hand to streaming.
    static Frustum look(const Vec3& eye, const Vec3& at)
    {
        const Mat4 view_projection =
            perspective(radians(60.0f), 16.0f / 9.0f, 0.1f, 50.0f)
            * look_at(eye, at, Vec3(0.0f, 1.0f, 0.0f));
        return make_frustum(view_projection);
    }

    /// @brief
    /// Purpose: Strips per-instantiation uuids so two writes of the same content compare equal.
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

    /// @brief
    /// Purpose: A written kit file parsed back, normalized for comparison.
    static Json parse_kit_file(const std::filesystem::path& path)
    {
        const auto text = read_text(path);
        return text ? Json::parse(*text, nullptr, false) : Json();
    }

    TEST(Sandbox, SpawnBuildsFluentToyWithBlocksAndStickers)
    {
        // Arrange
        register_ecs_test_blocks();
        auto sandbox = Sandbox();

        // Act
        Toy grunt = sandbox.add("Grunt")
                             .with(Transform {.position = Vec3(1.0f, 2.0f, 3.0f)})
                             .with(TestHealth {.hp = 50.0f, .armor = 10.0f})
                             .add("enemy");

        // Assert
        EXPECT_TRUE(grunt.is_alive());
        EXPECT_EQ(grunt.get_name(), "Grunt");
        EXPECT_TRUE(grunt.has<TestHealth>());
        EXPECT_TRUE(grunt.has("enemy"));
        EXPECT_FALSE(grunt.has("pickup"));
        EXPECT_EQ(grunt.add<TestHealth>().hp, 50.0f);
        EXPECT_TRUE(grunt.get_uuid().is_valid());
    }

    TEST(Sandbox, RemoveKillsToyAndItsSubtree)
    {
        // Arrange
        auto sandbox = Sandbox();
        Toy parent = sandbox.add("Parent");
        Toy child = sandbox.add("Child");
        child.set_parent(parent);

        // Act
        sandbox.remove(parent);

        // Assert: remove cascades to the whole subtree.
        EXPECT_FALSE(parent.is_alive());
        EXPECT_FALSE(child.is_alive());
    }

    TEST(Sandbox, FindsToysBySearchAndSticker)
    {
        // Arrange
        auto sandbox = Sandbox();
        Toy grunt = sandbox.add("Grunt").add("enemy");
        sandbox.add("Crate");
        auto stickered = std::vector<std::string>();

        // Act
        const auto by_name = sandbox.find("Grunt");
        const auto by_uuid = sandbox.find(grunt.get_uuid());
        sandbox.for_each_with(
            "enemy",
            [&stickered](Toy toy)
            {
                stickered.push_back(toy.get_name());
            });
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
        // Arrange: a whole authored world written to disk, read back, and instantiated.
        register_ecs_test_blocks();
        auto world = TestWorld();
        auto source = Sandbox();
        Toy parent = source.add("Room")
                              .with(Transform {.position = Vec3(5.0f, 0.0f, 0.0f)})
                              .add("level");
        Toy child = source.add("Grunt").with(TestHealth {.hp = 33.0f, .armor = 1.0f});
        child.set_parent(parent);
        ASSERT_TRUE(serialize(source, world.root / "world.kit").has_value());

        // Act
        const auto kit = deserialize<Kit>(world.root / "world.kit");
        ASSERT_TRUE(kit.has_value()) << kit.error();
        auto target = Sandbox();
        const auto loaded = add_kit(target, world.runtime.assets, world.runtime.events, *kit);

        // Assert
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        EXPECT_EQ(target.get_toy_count(), 3u); // the kit-instance root wrapper + Room + Grunt
        const auto room = target.find("Room");
        const auto grunt = target.find("Grunt");
        ASSERT_TRUE(room.has_value());
        ASSERT_TRUE(grunt.has_value());
        EXPECT_TRUE(room->has("level"));
        EXPECT_EQ(Toy(*room).add<Transform>().position, Vec3(5.0f, 0.0f, 0.0f));
        EXPECT_EQ(Toy(*grunt).add<TestHealth>().hp, 33.0f);
        ASSERT_TRUE(grunt->get_parent().has_value());
        EXPECT_EQ(grunt->get_parent()->get_id(), room->get_id());
    }

    TEST(Sandbox, KitWriteIsStableAcrossRoundTrips)
    {
        // Arrange
        register_ecs_test_blocks();
        auto world = TestWorld();
        auto source = Sandbox();
        source.add("Thing").with(TestHealth {.hp = 7.0f, .armor = 2.0f});
        ASSERT_TRUE(serialize(source, world.root / "first.kit").has_value());

        // Act: read the kit back and write it again. (Instantiating into a world and
        // re-capturing is deliberately NOT identity — a captured instance collapses back to
        // a KitInstance reference — so the stable round trip is kit -> read -> write.)
        const auto kit = deserialize<Kit>(world.root / "first.kit");
        ASSERT_TRUE(kit.has_value()) << kit.error();
        ASSERT_TRUE(serialize(*kit, world.root / "second.kit").has_value());

        // Assert: identical content modulo per-instantiation uuids.
        EXPECT_EQ(
            normalize_kit(parse_kit_file(world.root / "first.kit")),
            normalize_kit(parse_kit_file(world.root / "second.kit")));
    }

    TEST(Sandbox, NestedKitsInstantiateRecursively)
    {
        // Arrange: prefab <- room <- level, three deep — each nesting is a toy wearing a
        // KitInstance block, positioned by its own transform.
        register_ecs_test_blocks();
        auto world = TestWorld();
        auto author = Sandbox();
        author.add("Pickup");
        ASSERT_TRUE(serialize(author, world.root / "prefab.kit").has_value());

        auto room = Kit();
        room.add("prefab_ref")
            .with(KitInstance {.kit = AssetHandle<Kit>("prefab.kit")})
            .get_transform()
            .position = Vec3(1.0f, 0.0f, 0.0f);
        ASSERT_TRUE(serialize(room, world.root / "room.kit").has_value());

        auto level = Kit();
        level.add("room_ref")
            .with(KitInstance {.kit = AssetHandle<Kit>("room.kit")})
            .get_transform()
            .position = Vec3(10.0f, 0.0f, 0.0f);
        ASSERT_TRUE(serialize(level, world.root / "level.kit").has_value());

        // Act
        auto sandbox = Sandbox();
        const auto loaded = add_kit(
            sandbox,
            world.runtime.assets,
            world.runtime.events,
            AssetHandle<Kit>("level.kit"),
            Vec3(100.0f, 0.0f, 0.0f));

        // Assert: the parent chain composes 100 + 10 + 1 in world space.
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        const auto pickup = sandbox.find("Pickup");
        ASSERT_TRUE(pickup.has_value());
        const Vec3 world_position = Vec3(pickup->get_world_transform() * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
        EXPECT_EQ(world_position, Vec3(111.0f, 0.0f, 0.0f));
    }

    TEST(Sandbox, KitReferenceCycleFailsAndRollsBack)
    {
        // Arrange: a's child kit is b, b's child kit is a.
        auto world = TestWorld();
        auto a = Kit();
        a.add("InsideA");
        a.add("a_to_b").with(KitInstance {.kit = AssetHandle<Kit>("b.kit")});
        ASSERT_TRUE(serialize(a, world.root / "a.kit").has_value());
        auto b = Kit();
        b.add("b_to_a").with(KitInstance {.kit = AssetHandle<Kit>("a.kit")});
        ASSERT_TRUE(serialize(b, world.root / "b.kit").has_value());
        auto sandbox = Sandbox();

        // Act
        const auto loaded =
            add_kit(sandbox, world.runtime.assets, world.runtime.events, AssetHandle<Kit>("a.kit"));

        // Assert: error mentions the cycle and no partial toys survive.
        ASSERT_FALSE(loaded.has_value());
        EXPECT_NE(loaded.error().find("cycle"), std::string::npos);
        EXPECT_EQ(sandbox.get_toy_count(), 0u);
    }

    TEST(Sandbox, KitReferenceToMissingAssetFails)
    {
        // Arrange: a child kit that references an asset that does not exist.
        auto world = TestWorld();
        auto broken = Kit();
        broken.add("bad_ref").with(KitInstance {.kit = AssetHandle<Kit>("missing.kit")});
        ASSERT_TRUE(serialize(broken, world.root / "broken.kit").has_value());
        auto sandbox = Sandbox();

        // Act
        const auto loaded = add_kit(
            sandbox,
            world.runtime.assets,
            world.runtime.events,
            AssetHandle<Kit>("broken.kit"));

        // Assert
        EXPECT_FALSE(loaded.has_value());
        EXPECT_EQ(sandbox.get_toy_count(), 0u);
    }

    TEST(Sandbox, UnknownBlockTypeIsSkippedNotFatal)
    {
        // Arrange: a .kit whose toy carries a block type this build does not know (an
        // editor-only widget, say) — reading it must skip the block, not fail the toy.
        auto world = TestWorld();
        ASSERT_TRUE(write_text(
                        (world.root / "widget.kit").string(),
                        R"({"toys":[{"uuid":"0000000000000000000000000000abcd",)"
                        R"("name":"Survivor","blocks":[{"type":"EditorOnlyWidget"}]}]})")
                        .has_value());
        auto sandbox = Sandbox();

        // Act
        const auto kit = deserialize<Kit>(world.root / "widget.kit");
        ASSERT_TRUE(kit.has_value()) << kit.error();
        const auto loaded =
            add_kit(sandbox, world.runtime.assets, world.runtime.events, *kit);

        // Assert
        ASSERT_TRUE(loaded.has_value()) << loaded.error();
        EXPECT_TRUE(sandbox.find("Survivor").has_value());
    }

    TEST(Sandbox, StreamedKitLoadsAndUnloadsWithHysteresis)
    {
        // Arrange: a level kit with one streamed child kit (a KitInstance toy, streamed=true)
        // whose bounds sit at the origin with radius 0. Streamed by camera frustums (load
        // margin 5, unload margin 15).
        register_ecs_test_blocks();
        auto world = TestWorld();
        auto author = Sandbox();
        author.add("RoomToy");
        ASSERT_TRUE(serialize(author, world.root / "room.kit").has_value());
        auto level = Kit();
        level.add("far_room")
            .with(KitInstance {.kit = AssetHandle<Kit>("room.kit"), .streamed = true});
        auto sandbox = Sandbox();
        open(sandbox, level);
        world.flush_open(sandbox);
        EXPECT_FALSE(sandbox.find("RoomToy").has_value()); // streamed contents do not preload

        const auto wait_for = [&](bool present)
        {
            for (int i = 0; i < 500 && sandbox.find("RoomToy").has_value() != present; ++i)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                internal::update_jobs(world.runtime.jobs);
            }
        };

        // Act: a camera looking straight at the origin — the kit streams in (async).
        const auto seeing = std::array {look(Vec3(0.0f, 0.0f, 10.0f), Vec3(0.0f))};
        stream(sandbox, world.runtime.assets, world.runtime.events, world.runtime.jobs, seeing);
        wait_for(true);
        const bool loaded = sandbox.find("RoomToy").has_value();

        // A camera close by but looking AWAY: the origin is ~10 behind it — outside the +5
        // load volume but inside the +15 unload volume, so hysteresis keeps it loaded.
        const auto glancing = std::array {look(Vec3(0.0f, 0.0f, 10.0f), Vec3(0.0f, 0.0f, 20.0f))};
        stream(sandbox, world.runtime.assets, world.runtime.events, world.runtime.jobs, glancing);
        internal::update_jobs(world.runtime.jobs);
        const bool kept = sandbox.find("RoomToy").has_value();

        // Far away and looking away — out of every volume: unloads.
        const auto blind = std::array {look(Vec3(0.0f, 0.0f, 100.0f), Vec3(0.0f, 0.0f, 200.0f))};
        stream(sandbox, world.runtime.assets, world.runtime.events, world.runtime.jobs, blind);
        internal::update_jobs(world.runtime.jobs);
        const bool unloaded = sandbox.find("RoomToy").has_value();

        // ANY-frustum semantics: one blind camera plus one seeing camera loads it again.
        const auto split_screen = std::array {
            look(Vec3(0.0f, 0.0f, 100.0f), Vec3(0.0f, 0.0f, 200.0f)),
            look(Vec3(0.0f, 0.0f, 10.0f), Vec3(0.0f))};
        stream(
            sandbox, world.runtime.assets, world.runtime.events, world.runtime.jobs, split_screen);
        wait_for(true);

        // Assert
        EXPECT_TRUE(loaded);
        EXPECT_TRUE(kept); // hysteresis
        EXPECT_FALSE(unloaded);
        EXPECT_TRUE(sandbox.find("RoomToy").has_value()); // streamed back in
    }

    TEST(Sandbox, ImmediateChildKitsLoadOnTheFirstStreamTick)
    {
        // Arrange: a level kit with an immediate child kit (KitInstance toy, streamed=false).
        auto world = TestWorld();
        auto author = Sandbox();
        author.add("Skybox");
        ASSERT_TRUE(serialize(author, world.root / "sky.kit").has_value());
        auto level = Kit();
        level.add("sky").with(KitInstance {.kit = AssetHandle<Kit>("sky.kit")});
        auto sandbox = Sandbox();

        // Act: open defers; the first stream tick (even with no cameras) spawns the level.
        open(sandbox, level);
        world.flush_open(sandbox);

        // Assert
        EXPECT_TRUE(sandbox.find("Skybox").has_value());
    }

    TEST(Sandbox, ReadSandboxLoadsALevelKit)
    {
        // Arrange: a level kit on disk — read<Sandbox> turns it into a world.
        auto world = TestWorld();
        auto author = Sandbox();
        author.add("Skybox");
        ASSERT_TRUE(serialize(author, world.root / "sky.kit").has_value());
        auto level = Kit();
        level.add("sky").with(KitInstance {.kit = AssetHandle<Kit>("sky.kit")});
        ASSERT_TRUE(serialize(level, world.root / "level.kit").has_value());

        // Act
        auto from_kit = deserialize<Sandbox>(world.root / "level.kit");
        ASSERT_TRUE(from_kit.has_value()) << from_kit.error();
        world.flush_open(*from_kit);

        // Assert
        EXPECT_TRUE(from_kit->find("Skybox").has_value());
    }

    TEST(Sandbox, DisabledStatePersistsThroughKits)
    {
        // Arrange
        auto world = TestWorld();
        auto source = Sandbox();
        Toy toy = source.add("Lamp");
        toy.set_enabled(false);
        ASSERT_TRUE(serialize(source, world.root / "lamp.kit").has_value());

        // Act
        const auto kit = deserialize<Kit>(world.root / "lamp.kit");
        ASSERT_TRUE(kit.has_value()) << kit.error();
        auto target = Sandbox();
        ASSERT_TRUE(add_kit(target, world.runtime.assets, world.runtime.events, *kit).has_value());

        // Assert
        const auto reloaded = target.find("Lamp");
        ASSERT_TRUE(reloaded.has_value());
        EXPECT_FALSE(Toy(*reloaded).is_enabled());
        EXPECT_TRUE(source.add("Fresh").is_enabled()); // default stays on
    }

    TEST(Sandbox, CloseEmptiesTheSandboxForReopening)
    {
        // Arrange
        auto world = TestWorld();
        auto author = Sandbox();
        author.add("Skybox");
        ASSERT_TRUE(serialize(author, world.root / "sky.kit").has_value());
        auto level = Kit();
        level.add("sky").with(KitInstance {.kit = AssetHandle<Kit>("sky.kit")});
        auto sandbox = Sandbox();
        open(sandbox, level);
        world.flush_open(sandbox);
        ASSERT_TRUE(sandbox.find("Skybox").has_value());

        // Act
        close(sandbox);

        // Assert: empty, and a fresh open works again.
        EXPECT_EQ(sandbox.get_toy_count(), 0u);
        open(sandbox, level);
        world.flush_open(sandbox);
        EXPECT_TRUE(sandbox.find("Skybox").has_value());
    }

    TEST(Sandbox, WorldMatrixComposesParentChain)
    {
        // Arrange
        auto sandbox = Sandbox();
        Toy parent =
            sandbox.add("Parent").with(Transform {.position = Vec3(10.0f, 0.0f, 0.0f)});
        Toy child =
            sandbox.add("Child").with(Transform {.position = Vec3(0.0f, 5.0f, 0.0f)});
        child.set_parent(parent);

        // Act
        const Mat4 world = child.get_world_transform();
        const Vec4 origin = world * Vec4(0.0f, 0.0f, 0.0f, 1.0f);

        // Assert
        EXPECT_NEAR(origin.x, 10.0f, 0.0001f);
        EXPECT_NEAR(origin.y, 5.0f, 0.0001f);
    }
}
