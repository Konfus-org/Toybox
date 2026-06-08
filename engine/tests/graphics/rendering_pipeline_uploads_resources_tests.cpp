#include "rendering_pipeline_test_support.h"

namespace tbx::tests::graphics
{
    TEST(RenderingPipelineTests, Execute_ExpandsUploadedCullBoundsForValidMesh)
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
        auto mesh = world->create_entity("Triangle");
        mesh.add_component<Transform>(Vec3(0.0F, 0.0F, 0.0F));
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Pbr.mat")));
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
        ASSERT_EQ(instance_count, 1U);
        EXPECT_LT(instances[0].bounds_min.x, -0.5F);
        EXPECT_LT(instances[0].bounds_min.y, -0.5F);
        EXPECT_LT(instances[0].bounds_min.z, 0.0F);
        EXPECT_GT(instances[0].bounds_max.x, 0.5F);
        EXPECT_GT(instances[0].bounds_max.y, 0.5F);
        EXPECT_GT(instances[0].bounds_max.z, 0.0F);
    }

    TEST(RenderingPipelineTests, Execute_DoesNotUploadCullBoundsWithoutRenderableInstances)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto registry = make_rendering_registry();
        auto asset_manager = AssetManager(dispatcher, registry, std::filesystem::path());
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto world_manager = WorldManager(asset_manager_service);
        ASSERT_TRUE(world_manager.set_active_world(std::make_shared<World>()));
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
        EXPECT_EQ(instances, nullptr);
        EXPECT_EQ(instance_count, 0U);
    }

    TEST(RenderingPipelineTests, Execute_RepeatedFramesReuseUploadedTextures)
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
        auto mesh = world->create_entity("Triangle");
        mesh.add_component<Transform>(Vec3(0.0F, 0.0F, 0.0F));
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Textured.mat")));
        ASSERT_TRUE(world_manager.set_active_world(world));
        auto window_manager = RecordingWindowManager();
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            asset_manager_service,
            window_manager_service,
            world_manager_service);

        // Act
        const auto first_result = pipeline.execute(backend, GraphicsSettings(), DeltaTime());
        const auto texture_writes_after_first_frame = backend.texture_write_count;
        const auto second_result = pipeline.execute(backend, GraphicsSettings(), DeltaTime());

        // Assert
        EXPECT_TRUE(first_result.succeeded());
        EXPECT_TRUE(second_result.succeeded());
        EXPECT_GT(texture_writes_after_first_frame, 0U);
        EXPECT_EQ(backend.texture_write_count, texture_writes_after_first_frame);
    }

    TEST(RenderingPipelineTests, Execute_RepeatedFramesReuseUploadedGeometryBuffers)
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
        auto mesh = world->create_entity("Triangle");
        mesh.add_component<Transform>(Vec3(0.0F, 0.0F, 0.0F));
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Pbr.mat")));
        ASSERT_TRUE(world_manager.set_active_world(world));
        auto window_manager = RecordingWindowManager();
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            asset_manager_service,
            window_manager_service,
            world_manager_service);

        // Act
        const auto first_result = pipeline.execute(backend, GraphicsSettings(), DeltaTime());
        const auto vertex_writes_after_first_frame =
            count_buffer_writes(backend, "global_vertices");
        const auto index_writes_after_first_frame =
            count_buffer_writes(backend, "global_indices");
        const auto mesh_writes_after_first_frame = count_buffer_writes(backend, "global_meshes");
        const auto second_result = pipeline.execute(backend, GraphicsSettings(), DeltaTime());

        // Assert
        EXPECT_TRUE(first_result.succeeded());
        EXPECT_TRUE(second_result.succeeded());
        EXPECT_GT(vertex_writes_after_first_frame, 0U);
        EXPECT_GT(index_writes_after_first_frame, 0U);
        EXPECT_GT(mesh_writes_after_first_frame, 0U);
        EXPECT_EQ(count_buffer_writes(backend, "global_vertices"), vertex_writes_after_first_frame);
        EXPECT_EQ(count_buffer_writes(backend, "global_indices"), index_writes_after_first_frame);
        EXPECT_EQ(count_buffer_writes(backend, "global_meshes"), mesh_writes_after_first_frame);
    }

    TEST(RenderingPipelineTests, Execute_DirtyDynamicMeshRefreshesUploadedGeometryBuffers)
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
        auto mesh = world->create_entity("Triangle");
        mesh.add_component<Transform>(Vec3(0.0F, 0.0F, 0.0F));
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Pbr.mat")));
        ASSERT_TRUE(world_manager.set_active_world(world));
        auto window_manager = RecordingWindowManager();
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            asset_manager_service,
            window_manager_service,
            world_manager_service);

        // Act
        const auto first_result = pipeline.execute(backend, GraphicsSettings(), DeltaTime());
        const auto vertex_writes_after_first_frame =
            count_buffer_writes(backend, "global_vertices");
        auto& dynamic_mesh = mesh.get_component<DynamicMesh>();
        auto& editable_mesh = dynamic_mesh.edit_mesh();
        editable_mesh.vertices.vertices[0] = -0.75F;
        const auto second_result = pipeline.execute(backend, GraphicsSettings(), DeltaTime());

        // Assert
        EXPECT_TRUE(first_result.succeeded());
        EXPECT_TRUE(second_result.succeeded());
        EXPECT_GT(vertex_writes_after_first_frame, 0U);
        EXPECT_GT(count_buffer_writes(backend, "global_vertices"), vertex_writes_after_first_frame);
        EXPECT_GT(count_buffer_writes(backend, "global_indices"), 1U);
        EXPECT_GT(count_buffer_writes(backend, "global_meshes"), 1U);
    }
}
