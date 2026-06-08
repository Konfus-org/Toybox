#include "rendering_pipeline_test_support.h"

namespace tbx::tests::graphics
{
    TEST(RenderingPipelineTests, Execute_SkyEntityLoadsFallbackSkyMeshWhenPresent)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto tracking = std::make_shared<AssetLoadTracking>();
        auto registry = make_rendering_registry(tracking);
        auto asset_manager = AssetManager(dispatcher, registry, std::filesystem::path());
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto world_manager = WorldManager(asset_manager_service);
        auto world = std::make_shared<World>();
        auto camera = world->create_entity("Camera");
        camera.add_component<Transform>(Vec3(0.0F, 0.0F, 3.0F));
        camera.add_component<Camera>();
        auto sky = world->create_entity("Sky");
        sky.add_component<Transform>(Vec3(0.0F, 0.0F, 0.0F));
        sky.add_component<Sky>(Sky(MaterialInstance(Handle("Materials/Sky.mat"))));
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
        EXPECT_NE(
            std::find(
                tracking->material_paths.begin(),
                tracking->material_paths.end(),
                "C:/Users/jercl/Projects/Toybox/Engine/resources/Materials/Magenta.mat"),
            tracking->material_paths.end());
        EXPECT_NE(
            std::find(
                tracking->model_paths.begin(),
                tracking->model_paths.end(),
                "C:/Users/jercl/Projects/Toybox/Engine/resources/Models/Question.fbx"),
            tracking->model_paths.end());
    }

    TEST(RenderingPipelineTests, Execute_WithoutSkyEntityDoesNotLoadFallbackSkyMesh)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto tracking = std::make_shared<AssetLoadTracking>();
        auto registry = make_rendering_registry(tracking);
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
        EXPECT_EQ(
            std::find(
                tracking->model_paths.begin(),
                tracking->model_paths.end(),
                "C:/Users/jercl/Projects/Toybox/Engine/resources/Models/Question.fbx"),
            tracking->model_paths.end());
    }
}
