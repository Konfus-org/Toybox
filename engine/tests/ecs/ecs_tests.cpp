#include "ecs_tests.generated.h"
#include "in_memory_file_ops.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/world/manager.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/component.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/transform.h"

namespace tbx::tests::ecs
{
    [[serializable]];
    struct TestComponent : Component
    {
        [[prop]]
        int value = 0;
    };
}

namespace tbx::tests::ecs
{
    struct UnserializedComponent : Component
    {
        int value = 0;
    };

    class NullMessageDispatcher final : public IMessageDispatcher
    {
      protected:
        Result send(Message&) const override
        {
            return {};
        }

        std::shared_future<Result> post(std::unique_ptr<Message>) const override
        {
            auto promise = std::promise<Result> {};
            promise.set_value(Result());
            return promise.get_future().share();
        }
    };

    static std::shared_ptr<IMessageDispatcher> get_null_dispatcher()
    {
        static auto dispatcher = std::make_shared<NullMessageDispatcher>();
        return dispatcher;
    }

    // Validates parent metadata roundtrip via id-based accessors.
    TEST(ECSTests, CreatesEntityWithDescription)
    {
        // Arrange
        World world = {};

        // Act
        auto entity = world.create_global_entity("Player");
        entity.set_tag("Hero");
        entity.set_layer("Gameplay");

        // Assert
        EXPECT_EQ(entity.get_name(), "Player");
        EXPECT_EQ(entity.get_tag(), "Hero");
        EXPECT_EQ(entity.get_layer(), "Gameplay");

        Uuid parent = Uuid(99U);
        entity.set_parent(parent);
        EXPECT_EQ(entity.get_parent(), parent);
    }

    // Validates resolving a parent entity handle from a child entity.
    TEST(ECSTests, TryGetParentEntity_ReturnsParentEntity)
    {
        // Arrange
        World world = {};
        auto parent = world.create_global_entity("PlayerRoot");
        auto child = world.create_entity("PlayerVisual", parent.get_id());

        // Act
        auto resolved_parent = Entity {};
        const bool has_parent = child.try_get_parent_entity(resolved_parent);

        // Assert
        EXPECT_TRUE(has_parent);
        EXPECT_EQ(resolved_parent.get_id(), parent.get_id());
        EXPECT_EQ(resolved_parent.get_name(), "PlayerRoot");
    }

    // Validates registry-level entity existence checks by id.
    TEST(ECSTests, HasById_TracksEntityLifetime)
    {
        // Arrange
        World world = {};
        const auto entity = world.create_global_entity("LifetimeProbe");
        const auto entity_id = entity.get_id();

        // Act
        const bool has_before_destroy = world.has(entity_id);
        auto entity_to_destroy = world.get(entity_id);
        world.destroy(entity_to_destroy);
        const bool has_after_destroy = world.has(entity_id);
        const bool has_invalid_id = world.has(Uuid());

        // Assert
        EXPECT_TRUE(has_before_destroy);
        EXPECT_FALSE(has_after_destroy);
        EXPECT_FALSE(has_invalid_id);
    }

    TEST(ECSTests, WorldFindEndpoints_ReturnMatchingEntity)
    {
        // Arrange
        World world = {};
        auto player = world.create_global_entity("Character");
        player.set_tag("player");
        auto camera = world.create_entity("Camera", player.get_id());
        camera.set_tag("camera");

        // Act
        const auto found_by_id = world.get(player.get_id());
        const auto found_by_name = world.find_by_name("Camera");
        const auto found_by_tag = world.find_by_tag("player");
        const auto missing_entity = world.find_by_tag("missing");

        // Assert
        EXPECT_EQ(found_by_id.get_id(), player.get_id());
        EXPECT_EQ(found_by_name.get_id(), camera.get_id());
        EXPECT_EQ(found_by_tag.get_id(), player.get_id());
        EXPECT_FALSE(missing_entity.get_id().is_valid());
    }

    TEST(ECSTests, ComponentId_IsGeneratedAndPreservedWhenStored)
    {
        // Arrange
        World world = {};
        auto entity = world.create_global_entity("ComponentIdentity");
        auto component = TestComponent {};
        component.value = 42;
        const auto component_id = component.id;

        // Act
        auto& stored = entity.add_component<TestComponent>(component);

        // Assert
        EXPECT_TRUE(component_id.is_valid());
        EXPECT_EQ(stored.id, component_id);
        EXPECT_EQ(stored.value, 42);
    }

    TEST(ECSTests, WorldRuntime_AddsAndRemovesStreamedEntities)
    {
        // Arrange
        World world = {};
        auto registry = EntityRegistry();
        const auto id = registry.add(Uuid(48U), "Crate");
        const auto source = registry.get(id);

        // Act
        world.add_entities({source});
        const bool has_loaded_entity = world.has(Uuid(48U));
        world.remove_entities({Uuid(48U)});

        // Assert
        EXPECT_TRUE(has_loaded_entity);
        EXPECT_FALSE(world.has(Uuid(48U)));
    }

    TEST(ECSTests, WorldRuntime_LoadsGlobalsAsResidentEntities)
    {
        // Arrange
        auto registry = EntityRegistry();
        const auto id = registry.add(Uuid(49U), "Sun");
        auto globals = WorldGlobals {};
        globals.entities.push_back(registry.get(id));
        World world = {};

        // Act
        world.load_globals(globals);

        // Assert
        EXPECT_TRUE(world.has(Uuid(49U)));
        EXPECT_TRUE(world.is_global(Uuid(49U)));
    }

    TEST(ECSTests, EntitySerialization_RoundtripsMetadataAndRegisteredComponents)
    {
        // Arrange
        auto registry = EntityRegistry();
        const auto id = registry.add(Uuid(70U), "Probe", "tag", "layer", Uuid(71U));
        auto entity = registry.get(id);
        auto component = TestComponent();
        component.value = 25;
        entity.add_component<TestComponent>(component);

        // Act
        const auto json = JsonParser::parse(Entity::serialize(entity));
        auto roundtripped = Entity();
        const bool deserialized = Entity::deserialize(json.dump(), roundtripped);

        // Assert
        EXPECT_TRUE(deserialized);
        EXPECT_EQ(roundtripped.get_id(), Uuid(70U));
        EXPECT_EQ(roundtripped.get_name(), "Probe");
        EXPECT_EQ(roundtripped.get_tag(), "tag");
        EXPECT_EQ(roundtripped.get_layer(), "layer");
        EXPECT_EQ(roundtripped.get_parent(), Uuid(71U));
        ASSERT_TRUE(roundtripped.has_component<TestComponent>());
        EXPECT_EQ(roundtripped.get_component<TestComponent>().value, 25);
    }

    TEST(ECSTests, EntitySerialization_SkipsUnregisteredComponents)
    {
        // Arrange
        auto registry = EntityRegistry();
        const auto id = registry.add(Uuid(72U), "Probe");
        auto entity = registry.get(id);
        auto component = UnserializedComponent();
        component.value = 12;
        entity.add_component<UnserializedComponent>(component);

        // Act
        const auto json = JsonParser::parse(Entity::serialize(entity));
        const auto components = json.find("components");
        const bool has_components = components != json.end();

        // Assert
        EXPECT_TRUE(has_components);
        EXPECT_TRUE(components->empty());
    }

    TEST(ECSTests, EntitySerialization_SkipsUnknownComponentKeys)
    {
        // Arrange
        auto entity = Entity();

        // Act
        const bool deserialized = Entity::deserialize(
            R"({
                "id": { "value": 73 },
                "name": "Probe",
                "tag": "",
                "layer": "",
                "parent": { "value": 0 },
                "components": {
                    "missing_component": {
                        "value": 1
                    }
                }
            })",
            entity);

        // Assert
        EXPECT_TRUE(deserialized);
        EXPECT_EQ(entity.get_id(), Uuid(73U));
    }

    TEST(ECSTests, WorldAsset_LoadsPersistentEntities)
    {
        // Arrange
        auto file_ops = std::make_shared<::tbx::tests::InMemoryFileOps>("/virtual/worlds");
        file_ops->set_text("main.world.meta", R"({ "id": 16, "version": 1 })");
        file_ops->set_text(
            "main.world",
            R"({
                "globals": { "name": "main.globals", "id": { "value": 18 } },
                "chunks": []
            })");
        file_ops->set_text("main.globals.meta", R"({ "id": 18, "version": 1 })");
        file_ops->set_text(
            "main.globals",
            R"({
                "entities": [
                    {
                        "id": { "value": 32 },
                        "name": "Sun",
                        "tag": "",
                        "layer": "",
                        "parent": { "value": 0 },
                        "components": {
                            "directional_light": {
                                "id": { "value": 33 },
                                "color": { "r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0 },
                                "intensity": 2.0,
                                "cast_shadows": true,
                                "ambient": 0.25
                            }
                        }
                    }
                ]
            })");
        auto serialization_registry = SerializationRegistry(file_ops);

        // Act
        const auto world = serialization_registry.read<World>("main.world");
        const auto globals = serialization_registry.read<WorldGlobals>("main.globals");

        // Assert
        ASSERT_NE(world, nullptr);
        ASSERT_NE(globals, nullptr);
        world->load_globals(*globals);
        const auto entity = world->get(Uuid(32U));
        EXPECT_TRUE(entity.get_id().is_valid());
        EXPECT_TRUE(world->is_global(entity.get_id()));
        ASSERT_TRUE(entity.has_component<DirectionalLight>());
        EXPECT_FLOAT_EQ(entity.get_component<DirectionalLight>().ambient, 0.25F);
    }

    TEST(ECSTests, WorldAsset_RebindsPersistentEntityParents)
    {
        // Arrange
        auto file_ops = std::make_shared<::tbx::tests::InMemoryFileOps>("/virtual/worlds");
        file_ops->set_text("main.world.meta", R"({ "id": 17, "version": 1 })");
        file_ops->set_text(
            "main.world",
            R"({
                "globals": { "name": "main.globals", "id": { "value": 19 } },
                "chunks": []
            })");
        file_ops->set_text("main.globals.meta", R"({ "id": 19, "version": 1 })");
        file_ops->set_text(
            "main.globals",
            R"({
                "entities": [
                    {
                        "id": { "value": 32 },
                        "name": "Character",
                        "tag": "player",
                        "layer": "",
                        "parent": { "value": 0 },
                        "components": {
                            "transform": {
                                "id": { "value": 33 },
                                "position": [0.0, 10.0, 0.0],
                                "rotation": [0.0, 0.0, 0.0, 1.0],
                                "scale": [1.0, 1.0, 1.0]
                            }
                        }
                    },
                    {
                        "id": { "value": 34 },
                        "name": "Camera",
                        "tag": "camera",
                        "layer": "",
                        "parent": { "value": 32 },
                        "components": {
                            "camera": {
                                "id": { "value": 35 }
                            },
                            "transform": {
                                "id": { "value": 36 },
                                "position": [0.0, 0.0, 0.0],
                                "rotation": [0.0, 0.0, 0.0, 1.0],
                                "scale": [1.0, 1.0, 1.0]
                            }
                        }
                    }
                ]
            })");
        auto serialization_registry = SerializationRegistry(file_ops);

        // Act
        const auto world = serialization_registry.read<World>("main.world");
        const auto globals = serialization_registry.read<WorldGlobals>("main.globals");

        // Assert
        ASSERT_NE(world, nullptr);
        ASSERT_NE(globals, nullptr);
        world->load_globals(*globals);
        const auto player = world->find_by_tag("player");
        const auto camera = world->find_by_name("Camera");
        auto parent = Entity();
        ASSERT_TRUE(player.get_id().is_valid());
        ASSERT_TRUE(camera.try_get_parent_entity(parent));
        EXPECT_EQ(parent.get_id(), player.get_id());
        EXPECT_FLOAT_EQ(get_world_space_transform(camera).position.y, 10.0F);
    }

    TEST(ECSTests, WorldManager_LoadsFullChunkWithinUnloadRadius)
    {
        // Arrange
        auto file_ops = std::make_shared<::tbx::tests::InMemoryFileOps>("/virtual/worlds");
        file_ops->set_text("main.world.meta", R"({ "id": 32, "version": 1 })");
        file_ops->set_text(
            "main.world",
            R"({
                "globals": { "name": "main.globals", "id": { "value": 51 } },
                "chunks": [
                    { "name": "chunks/full.chunk", "id": { "value": 80 } }
                ]
            })");
        file_ops->set_text("main.globals.meta", R"({ "id": 51, "version": 1 })");
        file_ops->set_text(
            "main.globals",
            R"({
                "entities": [
                    {
                        "id": { "value": 40 },
                        "name": "Camera",
                        "tag": "",
                        "layer": "",
                        "parent": { "value": 0 },
                        "components": {
                            "camera": {
                                "id": { "value": 41 }
                            },
                            "transform": {
                                "id": { "value": 42 },
                                "position": [0.0, 0.0, 0.0],
                                "rotation": [0.0, 0.0, 0.0, 1.0],
                                "scale": [1.0, 1.0, 1.0]
                            }
                        }
                    }
                ]
            })");
        file_ops->set_text("chunks/full.chunk.meta", R"({ "id": 80, "version": 1 })");
        file_ops->set_text(
            "chunks/full.chunk",
            R"({
                "coord": { "x": 3, "y": 0, "z": 0 },
                "entities": [
                    {
                        "id": { "value": 60 },
                        "name": "FullTile",
                        "tag": "",
                        "layer": "",
                        "parent": { "value": 0 },
                        "components": {}
                    }
                ]
            })");
        auto serialization_registry = std::make_shared<SerializationRegistry>(file_ops);
        auto asset_manager = std::make_shared<AssetManager>(
            get_null_dispatcher(),
            serialization_registry,
            "/virtual/worlds",
            std::vector<std::filesystem::path> {"."},
            HandleSource(),
            file_ops);
        auto manager = WorldManager(asset_manager);
        ASSERT_EQ(asset_manager->ensure(Handle("main.globals", Uuid(51U))), Uuid(51U));
        ASSERT_EQ(asset_manager->ensure(Handle("chunks/full.chunk", Uuid(80U))), Uuid(80U));

        // Act
        const bool activated = manager.set_active_world(Handle("main.world", Uuid(0x20U)));
        manager.update(DeltaTime {.seconds = 0.016, .milliseconds = 16.0}, WorldSettings {});

        // Assert
        EXPECT_TRUE(activated);
        auto world = manager.get_active_world().lock();
        ASSERT_NE(world, nullptr);
        auto streamed_entity = world->get(Uuid(60U));
        ASSERT_TRUE(streamed_entity.get_id().is_valid());
        EXPECT_EQ(streamed_entity.get_name(), "FullTile");
    }

    TEST(ECSTests, WorldManager_SetActiveWorldFromHandle_StreamsActiveWorld)
    {
        // Arrange
        auto file_ops = std::make_shared<::tbx::tests::InMemoryFileOps>("/virtual/worlds");
        file_ops->set_text("main.world.meta", R"({ "id": 32, "version": 1 })");
        file_ops->set_text(
            "main.world",
            R"({
                "globals": { "name": "main.globals", "id": { "value": 51 } },
                "chunks": [
                    { "name": "chunks/full.chunk", "id": { "value": 80 } }
                ]
            })");
        file_ops->set_text("main.globals.meta", R"({ "id": 51, "version": 1 })");
        file_ops->set_text(
            "main.globals",
            R"({
                "entities": [
                    {
                        "id": { "value": 40 },
                        "name": "Camera",
                        "tag": "",
                        "layer": "",
                        "parent": { "value": 0 },
                        "components": {
                            "camera": {
                                "id": { "value": 41 }
                            },
                            "transform": {
                                "id": { "value": 42 },
                                "position": [0.0, 0.0, 0.0],
                                "rotation": [0.0, 0.0, 0.0, 1.0],
                                "scale": [1.0, 1.0, 1.0]
                            }
                        }
                    }
                ]
            })");
        file_ops->set_text("chunks/full.chunk.meta", R"({ "id": 80, "version": 1 })");
        file_ops->set_text(
            "chunks/full.chunk",
            R"({
                "coord": { "x": 3, "y": 0, "z": 0 },
                "entities": [
                    {
                        "id": { "value": 60 },
                        "name": "FullTile",
                        "tag": "",
                        "layer": "",
                        "parent": { "value": 0 },
                        "components": {}
                    }
                ]
            })");
        auto serialization_registry = std::make_shared<SerializationRegistry>(file_ops);
        auto asset_manager = std::make_shared<AssetManager>(
            get_null_dispatcher(),
            serialization_registry,
            "/virtual/worlds",
            std::vector<std::filesystem::path> {"."},
            HandleSource(),
            file_ops);
        auto manager = WorldManager(asset_manager);
        ASSERT_EQ(asset_manager->ensure(Handle("main.globals", Uuid(51U))), Uuid(51U));
        ASSERT_EQ(asset_manager->ensure(Handle("chunks/full.chunk", Uuid(80U))), Uuid(80U));

        // Act
        const bool activated = manager.set_active_world(Handle("main.world", Uuid(0x20U)));
        manager.update(DeltaTime {.seconds = 0.016, .milliseconds = 16.0}, WorldSettings {});

        // Assert
        EXPECT_TRUE(activated);
        auto world = manager.get_active_world().lock();
        ASSERT_NE(world, nullptr);
        auto streamed_entity = world->get(Uuid(60U));
        ASSERT_TRUE(streamed_entity.get_id().is_valid());
        EXPECT_EQ(streamed_entity.get_name(), "FullTile");
    }

    TEST(ECSTests, WorldManager_SetActiveWorldFromSharedPtr_OwnsProvidedWorld)
    {
        // Arrange
        auto serialization_registry = std::make_shared<SerializationRegistry>(
            std::make_shared<::tbx::tests::InMemoryFileOps>("/virtual/worlds"));
        auto asset_manager = std::make_shared<AssetManager>(
            get_null_dispatcher(),
            serialization_registry,
            "/virtual/worlds");
        auto manager = WorldManager(asset_manager);
        auto world = std::make_shared<World>();
        world->id = Uuid(90U);
        world->create_global_entity("RuntimeWorldEntity");

        // Act
        const bool activated = manager.set_active_world(world);
        auto weak_world = manager.get_active_world();
        world.reset();

        // Assert
        EXPECT_TRUE(activated);
        auto active_world = weak_world.lock();
        ASSERT_NE(active_world, nullptr);
        EXPECT_TRUE(active_world->find_by_name("RuntimeWorldEntity").get_id().is_valid());

        active_world.reset();
        manager.clear_active_world();
        EXPECT_TRUE(weak_world.expired());
    }

    TEST(ECSTests, WorldManager_SetActiveWorldFromMissingHandle_PreservesCurrentWorld)
    {
        // Arrange
        auto file_ops = std::make_shared<::tbx::tests::InMemoryFileOps>("/virtual/worlds");
        file_ops->set_text("main.world.meta", R"({ "id": 32, "version": 1 })");
        file_ops->set_text(
            "main.world",
            R"({
                "globals": { "name": "main.globals", "id": { "value": 51 } },
                "chunks": []
            })");
        file_ops->set_text("main.globals.meta", R"({ "id": 51, "version": 1 })");
        file_ops->set_text(
            "main.globals",
            R"({
                "entities": [
                    {
                        "id": { "value": 40 },
                        "name": "Existing",
                        "tag": "",
                        "layer": "",
                        "parent": { "value": 0 },
                        "components": {}
                    }
                ]
            })");
        auto serialization_registry = std::make_shared<SerializationRegistry>(file_ops);
        auto asset_manager = std::make_shared<AssetManager>(
            get_null_dispatcher(),
            serialization_registry,
            "/virtual/worlds",
            std::vector<std::filesystem::path> {"."},
            HandleSource(),
            file_ops);
        auto manager = WorldManager(asset_manager);
        ASSERT_EQ(asset_manager->ensure(Handle("main.globals", Uuid(51U))), Uuid(51U));
        ASSERT_TRUE(manager.set_active_world(Handle("main.world", Uuid(0x20U))));
        const auto original_world = manager.get_active_world().lock();

        // Act
        const bool activated = manager.set_active_world(Handle("missing.world", Uuid(0x21U)));

        // Assert
        EXPECT_FALSE(activated);
        EXPECT_EQ(manager.get_active_world().lock(), original_world);
    }
}
