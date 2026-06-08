#include "rendering_pipeline_test_support.h"

namespace tbx::tests::graphics
{
    TEST(RenderingPipelineTests, Execute_UploadsConfiguredShadowSettingsToGpuResources)
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
        auto light = world->create_entity("Sun");
        light.add_component<Transform>(Vec3(0.0F, 8.0F, 8.0F));
        light.add_component<DirectionalLight>(Color::WHITE, 2.0F);
        ASSERT_TRUE(world_manager.set_active_world(world));
        auto window_manager = RecordingWindowManager();
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            asset_manager_service,
            window_manager_service,
            world_manager_service);
        auto settings = GraphicsSettings();
        settings.shadow_map_resolution = 1024U;
        settings.shadow_softness = 2.5F;

        // Act
        const auto result = pipeline.execute(backend, settings, DeltaTime());
        const auto* uniforms = try_get_uploaded_frame_uniforms(backend);
        auto light_count = uint32(0U);
        const auto* lights = try_get_uploaded_light_buffer(backend, light_count);
        const auto shadow_atlas = find_created_texture(backend, "shadow_depth_atlas");

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(uniforms, nullptr);
        ASSERT_NE(lights, nullptr);
        ASSERT_TRUE(shadow_atlas.has_value());
        EXPECT_FLOAT_EQ(uniforms->shadow_settings.x, 1024.0F);
        EXPECT_FLOAT_EQ(uniforms->shadow_settings.z, 2.5F);
        EXPECT_EQ(shadow_atlas->get().size.width, 1024U);
        EXPECT_EQ(shadow_atlas->get().size.height, 1024U);
        ASSERT_GE(light_count, 1U);
        EXPECT_FLOAT_EQ(lights[0].shadow_data.x, 0.0F);
        EXPECT_FLOAT_EQ(lights[0].shadow_data.y, 2.5F);
        EXPECT_FALSE(has_created_buffer(backend, "global_shadows"));
    }

    TEST(RenderingPipelineTests, Execute_SanitizesInvalidShadowSettingsBeforeGpuUpload)
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
        auto light = world->create_entity("Sun");
        light.add_component<Transform>(Vec3(0.0F, 8.0F, 8.0F));
        light.add_component<DirectionalLight>(Color::WHITE, 2.0F);
        ASSERT_TRUE(world_manager.set_active_world(world));
        auto window_manager = RecordingWindowManager();
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            asset_manager_service,
            window_manager_service,
            world_manager_service);
        auto settings = GraphicsSettings();
        settings.shadow_map_resolution = 0U;
        settings.shadow_softness = -2.0F;

        // Act
        const auto result = pipeline.execute(backend, settings, DeltaTime());
        const auto* uniforms = try_get_uploaded_frame_uniforms(backend);
        auto light_count = uint32(0U);
        const auto* lights = try_get_uploaded_light_buffer(backend, light_count);
        const auto shadow_atlas = find_created_texture(backend, "shadow_depth_atlas");

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(uniforms, nullptr);
        ASSERT_NE(lights, nullptr);
        ASSERT_TRUE(shadow_atlas.has_value());
        EXPECT_FLOAT_EQ(uniforms->shadow_settings.x, 2048.0F);
        EXPECT_FLOAT_EQ(uniforms->shadow_settings.z, 0.0F);
        EXPECT_EQ(shadow_atlas->get().size.width, 2048U);
        EXPECT_EQ(shadow_atlas->get().size.height, 2048U);
        ASSERT_GE(light_count, 1U);
        EXPECT_FLOAT_EQ(lights[0].shadow_data.x, 0.0F);
        EXPECT_FLOAT_EQ(lights[0].shadow_data.y, 0.0F);
        EXPECT_FALSE(has_created_buffer(backend, "global_shadows"));
    }

    TEST(RenderingPipelineTests, Execute_UploadsSpotLightConeAnglesToLightBuffer)
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
        auto light = world->create_entity("SpotLight");
        light.add_component<Transform>(Vec3(0.0F, 2.0F, 2.0F));
        light.add_component<SpotLight>(Color::WHITE, 1.0F, 12.0F, 10.0F, 45.0F);
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
        auto light_count = uint32(0U);
        const auto* lights = try_get_uploaded_light_buffer(backend, light_count);

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(lights, nullptr);
        ASSERT_GE(light_count, 1U);
        EXPECT_FLOAT_EQ(lights[0].direction_type.w, static_cast<float>(SHADER_LIGHT_TYPE_SPOT));
        EXPECT_FLOAT_EQ(lights[0].spot_angles_area.x, 10.0F);
        EXPECT_FLOAT_EQ(lights[0].spot_angles_area.y, 45.0F);
    }

    TEST(RenderingPipelineTests, Execute_PointLightLeavesSpotAnglesZeroInLightBuffer)
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
        auto light = world->create_entity("PointLight");
        light.add_component<Transform>(Vec3(0.0F, 2.0F, 2.0F));
        light.add_component<PointLight>(Color::WHITE, 1.0F, 12.0F);
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
        auto light_count = uint32(0U);
        const auto* lights = try_get_uploaded_light_buffer(backend, light_count);

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(lights, nullptr);
        ASSERT_GE(light_count, 1U);
        EXPECT_FLOAT_EQ(lights[0].direction_type.w, static_cast<float>(SHADER_LIGHT_TYPE_POINT));
        EXPECT_FLOAT_EQ(lights[0].spot_angles_area.x, 0.0F);
        EXPECT_FLOAT_EQ(lights[0].spot_angles_area.y, 0.0F);
    }
}
