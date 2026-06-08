#include "rendering_pipeline_test_support.h"

namespace tbx::tests::graphics
{
    TEST(RenderingPipelineTests, Execute_SucceedsWhenFrameUniformUploadSucceeds)
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

        // Assert
        ASSERT_TRUE(result.succeeded());
        EXPECT_GE(backend.last_large_upload_size, sizeof(ShaderSceneUniforms));
    }

    TEST(RenderingPipelineTests, Execute_ChildCameraUsesParentWorldTransformForFrameUniforms)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto registry = make_rendering_registry();
        auto asset_manager = AssetManager(dispatcher, registry, std::filesystem::path());
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto world_manager = WorldManager(asset_manager_service);
        auto world = std::make_shared<World>();
        auto player = world->create_entity("Player");
        player.add_component<Transform>(
            Vec3(5.0F, 2.0F, 9.0F),
            Quat(Vec3(0.0F, to_radians(45.0F), 0.0F)));
        auto camera = world->create_entity("Camera", player.get_id());
        camera.add_component<Transform>(
            Vec3(0.0F, 1.5F, -2.0F),
            Quat(Vec3(to_radians(-15.0F), 0.0F, 0.0F)));
        camera.add_component<Camera>();
        auto mesh = world->create_entity("Triangle");
        mesh.add_component<Transform>(Vec3(5.0F, 2.0F, 0.0F));
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
        const auto* uniforms = try_get_uploaded_frame_uniforms(backend);

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(uniforms, nullptr);
        EXPECT_NEAR(uniforms->camera_position_time.x, 3.585786F, 0.001F);
        EXPECT_NEAR(uniforms->camera_position_time.y, 3.5F, 0.001F);
        EXPECT_NEAR(uniforms->camera_position_time.z, 7.585786F, 0.001F);
    }

    TEST(RenderingPipelineTests, Execute_CameraWithoutParentTransformFallsBackToLocalFrameUniforms)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto registry = make_rendering_registry();
        auto asset_manager = AssetManager(dispatcher, registry, std::filesystem::path());
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto world_manager = WorldManager(asset_manager_service);
        auto world = std::make_shared<World>();
        auto parent = world->create_entity("Parent");
        auto camera = world->create_entity("Camera", parent.get_id());
        camera.add_component<Transform>(Vec3(1.0F, 2.0F, 3.0F));
        camera.add_component<Camera>();
        auto mesh = world->create_entity("Triangle");
        mesh.add_component<Transform>(Vec3(1.0F, 2.0F, 0.0F));
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
        const auto* uniforms = try_get_uploaded_frame_uniforms(backend);

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(uniforms, nullptr);
        EXPECT_FLOAT_EQ(uniforms->camera_position_time.x, 1.0F);
        EXPECT_FLOAT_EQ(uniforms->camera_position_time.y, 2.0F);
        EXPECT_FLOAT_EQ(uniforms->camera_position_time.z, 3.0F);
    }

    TEST(RenderingPipelineTests, Execute_FailsWhenFrameUniformUploadFails)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        backend.fail_uniform_buffer_writes = true;
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

        // Assert
        ASSERT_FALSE(result.succeeded());
        EXPECT_EQ(result.get_report(), "uniform buffer upload failed");
    }
}
