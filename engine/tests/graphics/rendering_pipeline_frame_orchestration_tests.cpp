#include "rendering_pipeline_test_support.h"

namespace tbx::tests::graphics
{
    TEST(RenderingPipelineTests, Execute_WithRenderableWorldSubmitsGpuComputeAndBatchedDraws)
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
        const auto result = pipeline.execute(
            backend,
            GraphicsSettings(),
            DeltaTime {.seconds = 1.0 / 60.0, .milliseconds = 16.666666666666668});

        // Assert
        EXPECT_TRUE(result.succeeded());
        EXPECT_EQ(backend.begin_frame_count, 1U);
        EXPECT_EQ(backend.present_count, 1U);
        EXPECT_FALSE(backend.dispatches.empty());
        EXPECT_FALSE(backend.draw_calls.empty());
        EXPECT_GE(backend.barrier_count, 2U);
    }

    TEST(RenderingPipelineTests, Execute_EmptyWorldPresentsWithoutGpuWork)
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

        // Assert
        EXPECT_TRUE(result.succeeded());
        EXPECT_EQ(backend.begin_frame_count, 1U);
        EXPECT_EQ(backend.present_count, 1U);
        EXPECT_TRUE(backend.dispatches.empty());
        EXPECT_TRUE(backend.draw_calls.empty());
        EXPECT_TRUE(backend.indirect_draw_counts.empty());
    }
}
