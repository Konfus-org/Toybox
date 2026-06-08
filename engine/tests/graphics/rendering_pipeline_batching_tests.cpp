#include "rendering_pipeline_test_support.h"

namespace tbx::tests::graphics
{
    TEST(RenderingPipelineTests, Execute_RepeatedStaticMeshesSharingMaterialRenderSuccessfully)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto registry = make_rendering_registry();
        auto asset_manager = AssetManager(dispatcher, registry, std::filesystem::path());
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto world_manager = WorldManager(asset_manager_service);
        auto world = std::make_shared<World>();
        auto camera = world->create_entity("Camera");
        camera.add_component<Transform>(Vec3(0.0F, 0.0F, 3.0F));
        camera.add_component<Camera>();
        auto first_mesh = world->create_entity("TriangleA");
        first_mesh.add_component<Transform>(Vec3(-1.0F, 0.0F, 0.0F));
        first_mesh.add_component<StaticMesh>(StaticMesh(Handle("Models/Triangle.glb")));
        first_mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Pbr.mat")));
        auto second_mesh = world->create_entity("TriangleB");
        second_mesh.add_component<Transform>(Vec3(1.0F, 0.0F, 0.0F));
        second_mesh.add_component<StaticMesh>(StaticMesh(Handle("Models/Triangle.glb")));
        second_mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Pbr.mat")));
        ASSERT_TRUE(world_manager.set_active_world(world));
        auto window_manager = RecordingWindowManager();
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            asset_manager_service,
            window_manager_service,
            world_manager_service);

        // Act
        const auto result = pipeline.execute(backend, GraphicsSettings(), DeltaTime());

        // Assert
        EXPECT_TRUE(result.succeeded());
        EXPECT_FALSE(backend.dispatches.empty());
        ASSERT_EQ(backend.draw_calls.size(), 1U);
        EXPECT_EQ(backend.draw_calls.front().instance_count, 2U);
    }

    TEST(RenderingPipelineTests, Execute_BatchesEntitiesSharingMeshAndMaterialIntoOneDraw)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto registry = make_rendering_registry();
        auto asset_manager = AssetManager(dispatcher, registry, std::filesystem::path());
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto world_manager = WorldManager(asset_manager_service);
        auto world = std::make_shared<World>();
        auto camera = world->create_entity("Camera");
        camera.add_component<Transform>(Vec3(0.0F, 0.0F, 3.0F));
        camera.add_component<Camera>();
        auto first_mesh = world->create_entity("TriangleA");
        first_mesh.add_component<Transform>(Vec3(-1.0F, 0.0F, 0.0F));
        first_mesh.add_component<StaticMesh>(StaticMesh(Handle("Models/Triangle.glb")));
        first_mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Pbr.mat")));
        auto second_mesh = world->create_entity("TriangleB");
        second_mesh.add_component<Transform>(Vec3(1.0F, 0.0F, 0.0F));
        second_mesh.add_component<StaticMesh>(StaticMesh(Handle("Models/Triangle.glb")));
        second_mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Pbr.mat")));
        ASSERT_TRUE(world_manager.set_active_world(world));
        auto window_manager = RecordingWindowManager();
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            asset_manager_service,
            window_manager_service,
            world_manager_service);

        // Act
        const auto result = pipeline.execute(backend, GraphicsSettings(), DeltaTime());
        auto instance_count = uint32(0U);
        const auto* instances = try_get_uploaded_instance_buffer(backend, instance_count);

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(instances, nullptr);
        ASSERT_EQ(instance_count, 2U);
        EXPECT_EQ(instances[0].mesh_id, instances[1].mesh_id);
        ASSERT_EQ(backend.draw_calls.size(), 1U);
        EXPECT_EQ(backend.draw_calls.front().instance_count, 2U);
    }

    TEST(RenderingPipelineTests, Execute_KeepsDistinctMaterialOverridesAsSeparateDraws)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto registry = make_rendering_registry();
        auto asset_manager = AssetManager(dispatcher, registry, std::filesystem::path());
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto world_manager = WorldManager(asset_manager_service);
        auto world = std::make_shared<World>();
        auto camera = world->create_entity("Camera");
        camera.add_component<Transform>(Vec3(0.0F, 0.0F, 3.0F));
        camera.add_component<Camera>();
        auto first_mesh = world->create_entity("TriangleA");
        first_mesh.add_component<Transform>(Vec3(-1.0F, 0.0F, 0.0F));
        first_mesh.add_component<StaticMesh>(StaticMesh(Handle("Models/Triangle.glb")));
        auto first_material = MaterialInstance(Handle("Materials/Pbr.mat"));
        first_material.set_color(make_param_id("albedo_color"), Color(1.0F, 0.0F, 0.0F, 1.0F));
        first_mesh.add_component<MaterialInstance>(first_material);
        auto second_mesh = world->create_entity("TriangleB");
        second_mesh.add_component<Transform>(Vec3(1.0F, 0.0F, 0.0F));
        second_mesh.add_component<StaticMesh>(StaticMesh(Handle("Models/Triangle.glb")));
        auto second_material = MaterialInstance(Handle("Materials/Pbr.mat"));
        second_material.set_color(make_param_id("albedo_color"), Color(0.0F, 0.0F, 1.0F, 1.0F));
        second_mesh.add_component<MaterialInstance>(second_material);
        ASSERT_TRUE(world_manager.set_active_world(world));
        auto window_manager = RecordingWindowManager();
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            asset_manager_service,
            window_manager_service,
            world_manager_service);

        // Act
        const auto result = pipeline.execute(backend, GraphicsSettings(), DeltaTime());

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_EQ(backend.draw_calls.size(), 2U);
        EXPECT_EQ(backend.draw_calls[0].instance_count, 1U);
        EXPECT_EQ(backend.draw_calls[1].instance_count, 1U);
    }

    TEST(RenderingPipelineTests, Execute_BatchesMatchingMaterialOverridesIntoOneDraw)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto registry = make_rendering_registry();
        auto asset_manager = AssetManager(dispatcher, registry, std::filesystem::path());
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto world_manager = WorldManager(asset_manager_service);
        auto world = std::make_shared<World>();
        auto camera = world->create_entity("Camera");
        camera.add_component<Transform>(Vec3(0.0F, 0.0F, 3.0F));
        camera.add_component<Camera>();
        auto first_mesh = world->create_entity("TriangleA");
        first_mesh.add_component<Transform>(Vec3(-1.0F, 0.0F, 0.0F));
        first_mesh.add_component<StaticMesh>(StaticMesh(Handle("Models/Triangle.glb")));
        auto first_material = MaterialInstance(Handle("Materials/Pbr.mat"));
        first_material.set_color(make_param_id("albedo_color"), Color(1.0F, 0.0F, 0.0F, 1.0F));
        first_mesh.add_component<MaterialInstance>(first_material);
        auto second_mesh = world->create_entity("TriangleB");
        second_mesh.add_component<Transform>(Vec3(1.0F, 0.0F, 0.0F));
        second_mesh.add_component<StaticMesh>(StaticMesh(Handle("Models/Triangle.glb")));
        auto second_material = MaterialInstance(Handle("Materials/Pbr.mat"));
        second_material.set_color(make_param_id("albedo_color"), Color(1.0F, 0.0F, 0.0F, 1.0F));
        second_mesh.add_component<MaterialInstance>(second_material);
        ASSERT_TRUE(world_manager.set_active_world(world));
        auto window_manager = RecordingWindowManager();
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            asset_manager_service,
            window_manager_service,
            world_manager_service);

        // Act
        const auto result = pipeline.execute(backend, GraphicsSettings(), DeltaTime());
        auto instance_count = uint32(0U);
        const auto* instances = try_get_uploaded_instance_buffer(backend, instance_count);

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(instances, nullptr);
        ASSERT_EQ(instance_count, 2U);
        EXPECT_EQ(instances[0].mesh_id, instances[1].mesh_id);
        ASSERT_EQ(backend.draw_calls.size(), 1U);
        EXPECT_EQ(backend.draw_calls.front().instance_count, 2U);
    }

    TEST(RenderingPipelineTests, Execute_BatchesMatchingDynamicMeshGeometryIntoOneDraw)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto registry = make_rendering_registry();
        auto asset_manager = AssetManager(dispatcher, registry, std::filesystem::path());
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto world_manager = WorldManager(asset_manager_service);
        auto world = std::make_shared<World>();
        auto camera = world->create_entity("Camera");
        camera.add_component<Transform>(Vec3(0.0F, 0.0F, 3.0F));
        camera.add_component<Camera>();
        auto first_mesh = world->create_entity("TriangleA");
        first_mesh.add_component<Transform>(Vec3(-1.0F, 0.0F, 0.0F));
        first_mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        first_mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Pbr.mat")));
        auto second_mesh = world->create_entity("TriangleB");
        second_mesh.add_component<Transform>(Vec3(1.0F, 0.0F, 0.0F));
        second_mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        second_mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Pbr.mat")));
        ASSERT_TRUE(world_manager.set_active_world(world));
        auto window_manager = RecordingWindowManager();
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            asset_manager_service,
            window_manager_service,
            world_manager_service);

        // Act
        const auto result = pipeline.execute(backend, GraphicsSettings(), DeltaTime());

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_EQ(backend.draw_calls.size(), 1U);
        EXPECT_EQ(backend.draw_calls.front().instance_count, 2U);
    }

    TEST(RenderingPipelineTests, Execute_KeepsDistinctDynamicMeshGeometryAsSeparateDraws)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto registry = make_rendering_registry();
        auto asset_manager = AssetManager(dispatcher, registry, std::filesystem::path());
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto world_manager = WorldManager(asset_manager_service);
        auto world = std::make_shared<World>();
        auto camera = world->create_entity("Camera");
        camera.add_component<Transform>(Vec3(0.0F, 0.0F, 3.0F));
        camera.add_component<Camera>();
        auto first_mesh = world->create_entity("TriangleA");
        first_mesh.add_component<Transform>(Vec3(-1.0F, 0.0F, 0.0F));
        first_mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        first_mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Pbr.mat")));
        auto second_mesh = world->create_entity("TriangleB");
        second_mesh.add_component<Transform>(Vec3(1.0F, 0.0F, 0.0F));
        second_mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        second_mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Pbr.mat")));
        auto& second_dynamic_mesh = second_mesh.get_component<DynamicMesh>();
        second_dynamic_mesh.edit_mesh().vertices.vertices[0] = -0.25F;
        ASSERT_TRUE(world_manager.set_active_world(world));
        auto window_manager = RecordingWindowManager();
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            asset_manager_service,
            window_manager_service,
            world_manager_service);

        // Act
        const auto result = pipeline.execute(backend, GraphicsSettings(), DeltaTime());
        auto instance_count = uint32(0U);
        const auto* instances = try_get_uploaded_instance_buffer(backend, instance_count);

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(instances, nullptr);
        ASSERT_EQ(instance_count, 2U);
        EXPECT_NE(instances[0].mesh_id, instances[1].mesh_id);
        ASSERT_EQ(backend.draw_calls.size(), 2U);
        EXPECT_EQ(backend.draw_calls[0].instance_count, 1U);
        EXPECT_EQ(backend.draw_calls[1].instance_count, 1U);
    }

    TEST(RenderingPipelineTests, Execute_KeepsDistinctMeshGroupsAsSeparateDraws)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto registry = make_rendering_registry();
        auto asset_manager = AssetManager(dispatcher, registry, std::filesystem::path());
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto world_manager = WorldManager(asset_manager_service);
        auto world = std::make_shared<World>();
        auto camera = world->create_entity("Camera");
        camera.add_component<Transform>(Vec3(0.0F, 0.0F, 3.0F));
        camera.add_component<Camera>();
        auto first_mesh = world->create_entity("Triangle");
        first_mesh.add_component<Transform>(Vec3(-1.0F, 0.0F, 0.0F));
        first_mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        first_mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Pbr.mat")));
        auto second_mesh = world->create_entity("Model");
        second_mesh.add_component<Transform>(Vec3(1.0F, 0.0F, 0.0F));
        second_mesh.add_component<StaticMesh>(StaticMesh(Handle("Models/Triangle.glb")));
        second_mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Pbr.mat")));
        ASSERT_TRUE(world_manager.set_active_world(world));
        auto window_manager = RecordingWindowManager();
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            asset_manager_service,
            window_manager_service,
            world_manager_service);

        // Act
        const auto result = pipeline.execute(backend, GraphicsSettings(), DeltaTime());

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_EQ(backend.draw_calls.size(), 2U);
        EXPECT_EQ(backend.draw_calls[0].instance_count, 1U);
        EXPECT_EQ(backend.draw_calls[1].instance_count, 1U);
    }
}
