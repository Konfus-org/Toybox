#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/streamer.h"
#include "tbx/systems/files/in_memory_file_ops.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/component.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/transform.h"

namespace tbx::tests::ecs
{
    [[tbx::serializable]];
    [[tbx::prop(id, value)]];
    struct TestComponent : Component
    {
        int value = 0;
    };
}

#include "ecs_tests.generated.h"

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
            promise.set_value(Result {});
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
        auto entity = world.create_persistent_entity("Player");
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
        auto parent = world.create_persistent_entity("PlayerRoot");
        auto child = world.create_spatial_entity("PlayerVisual", parent.get_id());

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
        const auto entity = world.create_persistent_entity("LifetimeProbe");
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
        auto player = world.create_persistent_entity("Character");
        player.set_tag("player");
        auto camera = world.create_spatial_entity("Camera", player.get_id());
        camera.set_tag("camera");

        // Act
        const auto found_by_id = world.find_by_id(player.get_id());
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
        auto entity = world.create_persistent_entity("ComponentIdentity");
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

    TEST(ECSTests, PersistentEntities_DoNotEnterChunkMembership)
    {
        // Arrange
        World world = {};
        auto entity = world.create_persistent_entity("Sun");
        entity.add_component<Transform>();

        // Act
        world.update_chunk_membership();
        auto coord = WorldChunkCoord {};

        // Assert
        EXPECT_TRUE(world.is_persistent(entity.get_id()));
        EXPECT_FALSE(world.try_get_chunk(entity.get_id(), coord));
    }

    TEST(ECSTests, SpatialEntities_MoveBetweenChunksWhenTransformChanges)
    {
        // Arrange
        World world = {};
        world.chunk_size = 10.0F;
        auto entity = world.create_spatial_entity("Crate");
        auto& transform = entity.add_component<Transform>();
        transform.position = Vec3(2.0F, 0.0F, 2.0F);

        // Act
        world.update_chunk_membership();
        auto first_coord = WorldChunkCoord {};
        const bool has_first_coord = world.try_get_chunk(entity.get_id(), first_coord);
        transform.position = Vec3(21.0F, 0.0F, -11.0F);
        world.update_chunk_membership();
        auto second_coord = WorldChunkCoord {};
        const bool has_second_coord = world.try_get_chunk(entity.get_id(), second_coord);

        // Assert
        EXPECT_TRUE(has_first_coord);
        EXPECT_EQ(first_coord, WorldChunkCoord(0, 0, 0));
        EXPECT_TRUE(has_second_coord);
        EXPECT_EQ(second_coord, WorldChunkCoord(2, 0, -2));
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
        const auto json = JsonParser::parse(Serializer<Entity>::to_json(entity));
        auto roundtripped = Entity();
        const bool deserialized = Serializer<Entity>::from_json(json.dump(), roundtripped);

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
        const auto json = JsonParser::parse(Serializer<Entity>::to_json(entity));
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
        const bool deserialized = Serializer<Entity>::from_json(
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
                "chunk_size": 16.0,
                "persistent_entities": [
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
                ],
                "chunks": []
            })");
        auto serialization_registry = SerializationRegistry(file_ops);

        // Act
        const auto world = serialization_registry.read<World>("main.world");

        // Assert
        ASSERT_NE(world, nullptr);
        const auto entity = world->get(Uuid(32U));
        EXPECT_TRUE(entity.get_id().is_valid());
        EXPECT_TRUE(world->is_persistent(entity.get_id()));
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
                "chunk_size": 16.0,
                "persistent_entities": [
                    {
                        "id": { "value": 32 },
                        "name": "Character",
                        "tag": "player",
                        "layer": "",
                        "parent": { "value": 0 },
                        "components": {
                            "transform": {
                                "id": { "value": 33 },
                                "position": { "x": 0.0, "y": 10.0, "z": 0.0 },
                                "rotation": { "x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0 },
                                "scale": { "x": 1.0, "y": 1.0, "z": 1.0 }
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
                                "position": { "x": 0.0, "y": 0.0, "z": 0.0 },
                                "rotation": { "x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0 },
                                "scale": { "x": 1.0, "y": 1.0, "z": 1.0 }
                            }
                        }
                    }
                ],
                "chunks": []
            })");
        auto serialization_registry = SerializationRegistry(file_ops);

        // Act
        const auto world = serialization_registry.read<World>("main.world");

        // Assert
        ASSERT_NE(world, nullptr);
        const auto player = world->find_by_tag("player");
        const auto camera = world->find_by_name("Camera");
        auto parent = Entity();
        ASSERT_TRUE(player.get_id().is_valid());
        ASSERT_TRUE(camera.try_get_parent_entity(parent));
        EXPECT_EQ(parent.get_id(), player.get_id());
        EXPECT_FLOAT_EQ(get_world_space_transform(camera).position.y, 10.0F);
    }

    TEST(ECSTests, EntityStreamer_LoadsLowVisualChunkWithFullSimulation)
    {
        // Arrange
        auto file_ops = std::make_shared<::tbx::tests::InMemoryFileOps>("/virtual/worlds");
        file_ops->set_text("main.world.meta", R"({ "id": 32, "version": 1 })");
        file_ops->set_text(
            "main.world",
            R"({
                "chunk_size": 10.0,
                "full_visual_radius_chunks": 1,
                "low_visual_radius_chunks": 5,
                "simulation_radius_chunks": 4,
                "reduced_simulation_radius_chunks": 5,
                "unload_radius_chunks": 6,
                "persistent_entities": [
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
                                "position": { "x": 0.0, "y": 0.0, "z": 0.0 },
                                "rotation": { "x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0 },
                                "scale": { "x": 1.0, "y": 1.0, "z": 1.0 }
                            }
                        }
                    }
                ],
                "chunks": [
                    {
                        "coord": { "x": 3, "y": 0, "z": 0 },
                        "full_chunk": { "name": "chunks/full.chunk", "id": { "value": 50 } },
                        "low_lod_chunk": { "name": "chunks/low.chunk", "id": { "value": 51 } },
                        "simulation_chunk": { "name": "", "id": { "value": 0 } }
                    }
                ]
            })");
        file_ops->set_text("chunks/full.chunk.meta", R"({ "id": 80, "version": 1 })");
        file_ops->set_text("chunks/low.chunk.meta", R"({ "id": 81, "version": 1 })");
        file_ops->set_text(
            "chunks/full.chunk",
            R"({
                "coord": { "x": 3, "y": 0, "z": 0 },
                "lod": "full",
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
        file_ops->set_text(
            "chunks/low.chunk",
            R"({
                "coord": { "x": 3, "y": 0, "z": 0 },
                "lod": "low",
                "entities": [
                    {
                        "id": { "value": 60 },
                        "name": "LowTile",
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
            std::vector<std::filesystem::path>(),
            HandleSource(),
            file_ops);
        auto streamer = EntityStreamer(asset_manager);
        auto world = asset_manager->load<World>(Handle("main.world", Uuid(0x20U)));

        // Act
        streamer.update(DeltaTime {.seconds = 0.016, .milliseconds = 16.0});

        // Assert
        ASSERT_NE(world, nullptr);
        auto streamed_entity = world->get(Uuid(60U));
        ASSERT_TRUE(streamed_entity.get_id().is_valid());
        EXPECT_EQ(streamed_entity.get_name(), "LowTile");
        ASSERT_TRUE(streamed_entity.has_component<WorldSimulationState>());
        EXPECT_EQ(
            streamed_entity.get_component<WorldSimulationState>().mode,
            WorldSimulationMode::FULL);
    }
}
