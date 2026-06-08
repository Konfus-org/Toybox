#include "rendering_pipeline_test_support.h"

namespace tbx::tests::graphics
{
    TEST(RenderingPipelineTests, Execute_UsesMaterialDefinedRasterShader)
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
        mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Custom.mat")));
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
        const auto custom_pipeline = std::find_if(
            backend.raster_pipelines.begin(),
            backend.raster_pipelines.end(),
            [](const RasterPipelineDesc& desc)
            {
                return desc.shaders.size() == 2U
                       && desc.shaders[0].source.find("Shaders/Custom.vert") != std::string::npos
                       && desc.shaders[1].source.find("Shaders/Custom.frag") != std::string::npos;
            });
        ASSERT_NE(custom_pipeline, backend.raster_pipelines.end());
        ASSERT_EQ(backend.draw_calls.size(), 1U);
        EXPECT_EQ(backend.draw_calls.front().instance_count, 1U);
    }

    TEST(RenderingPipelineTests, Execute_DisablesCullingForTwoSidedMaterials)
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
        mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Custom.mat")));
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
        const auto custom_pipeline = std::find_if(
            backend.raster_pipelines.begin(),
            backend.raster_pipelines.end(),
            [](const RasterPipelineDesc& desc)
            {
                return desc.shaders.size() == 2U
                       && desc.shaders[0].source.find("Shaders/Custom.vert") != std::string::npos
                       && desc.shaders[1].source.find("Shaders/Custom.frag") != std::string::npos;
            });
        ASSERT_NE(custom_pipeline, backend.raster_pipelines.end());
        EXPECT_FALSE(custom_pipeline->is_culling_enabled);
    }

    TEST(RenderingPipelineTests, Execute_EnablesBackfaceCullingForOneSidedMaterials)
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
        const auto pbr_pipeline = std::find_if(
            backend.raster_pipelines.begin(),
            backend.raster_pipelines.end(),
            [](const RasterPipelineDesc& desc)
            {
                return desc.shaders.size() == 2U
                       && desc.shaders[0].source.find("Shaders/Pbr.vert") != std::string::npos
                       && desc.shaders[1].source.find("Shaders/Pbr.frag") != std::string::npos;
            });
        ASSERT_NE(pbr_pipeline, backend.raster_pipelines.end());
        EXPECT_TRUE(pbr_pipeline->is_culling_enabled);
        EXPECT_EQ(pbr_pipeline->cull_mode, GraphicsCullMode::BACK);
    }

    TEST(RenderingPipelineTests, Execute_UntexturedMaterialUploadsSemanticDefaultTextureSlots)
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
        const auto albedo = find_uploaded_texture(backend, "ToyboxDefaultAlbedo");
        const auto normal = find_uploaded_texture(backend, "ToyboxDefaultNormal");
        const auto scalar = find_uploaded_texture(backend, "ToyboxDefaultScalar");
        const auto emissive = find_uploaded_texture(backend, "ToyboxDefaultEmissive");

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_TRUE(albedo.has_value());
        ASSERT_TRUE(normal.has_value());
        ASSERT_TRUE(scalar.has_value());
        ASSERT_TRUE(emissive.has_value());
        EXPECT_EQ(albedo->get(), (std::vector<std::byte> {
                                     std::byte(0xFF),
                                     std::byte(0xFF),
                                     std::byte(0xFF),
                                     std::byte(0xFF),
                                 }));
        EXPECT_EQ(normal->get(), (std::vector<std::byte> {
                                     std::byte(0x80),
                                     std::byte(0x80),
                                     std::byte(0xFF),
                                     std::byte(0xFF),
                                 }));
        EXPECT_EQ(scalar->get(), (std::vector<std::byte> {
                                     std::byte(0xFF),
                                     std::byte(0xFF),
                                     std::byte(0xFF),
                                     std::byte(0xFF),
                                 }));
        EXPECT_EQ(emissive->get(), (std::vector<std::byte> {
                                       std::byte(0x00),
                                       std::byte(0x00),
                                       std::byte(0x00),
                                       std::byte(0xFF),
                                   }));
    }

    TEST(RenderingPipelineTests, Execute_MissingExplicitTextureHandleFallsBackToSemanticSlot)
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
        mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/MissingTextured.mat")));
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
        EXPECT_EQ(backend.texture_write_count, 4U);
        EXPECT_FALSE(find_uploaded_texture(backend, "Textures/DoesNotExist.tex").has_value());
    }

    TEST(RenderingPipelineTests, Execute_AllowsMixedRasterMaterialProgramsInSameFrame)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend();
        backend.require_index_rebind_for_raster_draw = true;
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
        first_mesh.add_component<Transform>(Vec3(0.0F, 0.0F, 0.0F));
        first_mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        first_mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Custom.mat")));
        auto second_mesh = world->create_entity("TriangleB");
        second_mesh.add_component<Transform>(Vec3(1.0F, 0.0F, 0.0F));
        second_mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        second_mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Other.mat")));
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
        const auto custom_pipeline = std::find_if(
            backend.raster_pipelines.begin(),
            backend.raster_pipelines.end(),
            [](const RasterPipelineDesc& desc)
            {
                return desc.shaders.size() == 2U
                       && desc.shaders[0].source.find("Shaders/Custom.vert") != std::string::npos
                       && desc.shaders[1].source.find("Shaders/Custom.frag") != std::string::npos;
            });
        const auto other_pipeline = std::find_if(
            backend.raster_pipelines.begin(),
            backend.raster_pipelines.end(),
            [](const RasterPipelineDesc& desc)
            {
                return desc.shaders.size() == 2U
                       && desc.shaders[0].source.find("Shaders/Other.vert") != std::string::npos
                       && desc.shaders[1].source.find("Shaders/Other.frag") != std::string::npos;
            });
        EXPECT_NE(custom_pipeline, backend.raster_pipelines.end());
        EXPECT_NE(other_pipeline, backend.raster_pipelines.end());
        ASSERT_EQ(backend.draw_calls.size(), 2U);
    }

    TEST(RenderingPipelineTests, Execute_FailsWhenMaterialHasNoValidRasterShader)
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
        mesh.add_component<MaterialInstance>(MaterialInstance(Handle("Materials/Broken.mat")));
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
        EXPECT_FALSE(result.succeeded());
        EXPECT_TRUE(result.get_report().find("missing a valid raster shader program") != std::string::npos);
    }

    TEST(RenderingPipelineTests, Execute_UsesMaterialContractToPopulateGpuMaterialBuffer)
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
        mesh.add_component<Transform>(Vec3(0.0F));
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        auto material = MaterialInstance(Handle("Materials/TexturedPbr.mat"));
        material.set_parameter("albedo_color", Color(0.25F, 0.5F, 0.75F, 1.0F));
        material.set_parameter("roughness", 0.35F);
        material.set_texture("albedo_map", Handle("Textures/Checker.tex"));
        mesh.add_component<MaterialInstance>(material);
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
        auto material_count = uint32(0U);
        const auto* materials = try_get_uploaded_material_buffer(backend, material_count);

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(materials, nullptr);
        ASSERT_GE(material_count, 1U);
        EXPECT_FLOAT_EQ(materials[0].base_color.x, 0.25F);
        EXPECT_FLOAT_EQ(materials[0].base_color.y, 0.5F);
        EXPECT_FLOAT_EQ(materials[0].base_color.z, 0.75F);
        EXPECT_FLOAT_EQ(materials[0].surface.y, 0.35F);
    }

    TEST(RenderingPipelineTests, Execute_FallsBackWhenMaterialOverrideTypeMismatchesSchema)
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
        mesh.add_component<Transform>(Vec3(0.0F));
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        auto material = MaterialInstance(Handle("Materials/Pbr.mat"));
        material.set_parameter("roughness", 1);
        mesh.add_component<MaterialInstance>(material);
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
        auto material_count = uint32(0U);
        const auto* materials = try_get_uploaded_material_buffer(backend, material_count);

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(materials, nullptr);
        ASSERT_GE(material_count, 1U);
        EXPECT_FLOAT_EQ(materials[0].base_color.x, 1.0F);
        EXPECT_FLOAT_EQ(materials[0].base_color.y, 0.0F);
        EXPECT_FLOAT_EQ(materials[0].base_color.z, 1.0F);
    }

    TEST(RenderingPipelineTests, Execute_IgnoresUndeclaredMaterialBindings)
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
        mesh.add_component<Transform>(Vec3(0.0F));
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        auto material = MaterialInstance(Handle("Materials/TexturedPbr.mat"));
        material.set_parameter("albedo_color", Color(0.4F, 0.3F, 0.2F, 1.0F));
        material.set_parameter("unused_scalar", 9.0F);
        mesh.add_component<MaterialInstance>(material);
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
        auto material_count = uint32(0U);
        const auto* materials = try_get_uploaded_material_buffer(backend, material_count);

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(materials, nullptr);
        ASSERT_GE(material_count, 1U);
        EXPECT_FLOAT_EQ(materials[0].base_color.x, 0.4F);
        EXPECT_FLOAT_EQ(materials[0].surface.x, 0.0F);
    }
}
