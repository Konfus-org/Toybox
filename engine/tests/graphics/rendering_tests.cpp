#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/ecs/world/manager.h"
#include "tbx/systems/graphics/rendering_pipeline.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/shader.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/trig.h"
#include "gtest/gtest.h"
#include <array>
#include <future>
#include <unordered_map>

namespace tbx::tests::graphics
{
    static MaterialParameter make_material_parameter(
        const char* name,
        MaterialParameterData data,
        MaterialBindingTarget target)
    {
        auto parameter = MaterialParameter(name, std::move(data));
        parameter.target = target;
        return parameter;
    }

    static MaterialTextureBinding make_material_texture_binding(
        const char* name,
        Handle texture,
        MaterialBindingTarget target)
    {
        auto binding = MaterialTextureBinding(name, std::move(texture));
        binding.target = target;
        return binding;
    }

    struct AssetLoadTracking final
    {
        std::vector<std::string> material_paths = {};
        std::vector<std::string> model_paths = {};
    };

    struct DrawCallRecord final
    {
        uint32 index_count = 0U;
        uint32 instance_count = 0U;
        uint32 first_index = 0U;
        int32 vertex_offset = 0;
        uint32 first_instance = 0U;
    };

    class NullMessageDispatcher final : public IMessageDispatcher
    {
      protected:
        Result send(Message&) const override
        {
            return Result();
        }

        std::shared_future<Result> post(std::unique_ptr<Message>) const override
        {
            auto promise = std::promise<Result>();
            promise.set_value(Result());
            return promise.get_future().share();
        }
    };

    class RecordingWindowManager final : public IWindowManager
    {
      public:
        Window open(const WindowCreateInfo& = {}) override
        {
            return window;
        }

        bool close(const Window&) override
        {
            return true;
        }

        bool has(const Window&) const override
        {
            return true;
        }

        bool is_open(const Window&) const override
        {
            return true;
        }

        WindowMode get_mode(const Window&) const override
        {
            return WindowMode::WINDOWED;
        }

        bool set_mode(const Window&, WindowMode) override
        {
            return true;
        }

        std::string get_title(const Window&) const override
        {
            return "test";
        }

        bool set_title(const Window&, std::string) override
        {
            return true;
        }

        NativeWindowHandle get_native_handle(const Window&) const override
        {
            return nullptr;
        }

        Size get_size(const Window&) const override
        {
            return Size {.width = 1280U, .height = 720U};
        }

        bool set_size(const Window&, const Size&) override
        {
            return true;
        }

        std::vector<Window> get_open_windows() const override
        {
            return {window};
        }

        bool has_main_window() const override
        {
            return true;
        }

        const Window& get_main_window() const override
        {
            return window;
        }

        bool set_main_window(const Window&) override
        {
            return true;
        }

        void update() override {}
        void shutdown() override {}

      public:
        Window window = Window("test");
    };

    class RecordingGraphicsBackend final : public IGraphicsBackend
    {
      public:
        GraphicsApi get_api() const override
        {
            return GraphicsApi::OPEN_GL;
        }

        VsyncMode get_vsync() const override
        {
            return VsyncMode::OFF;
        }

        Result set_vsync(VsyncMode) override
        {
            return Result();
        }

        Result begin_frame(const Window&) override
        {
            ++begin_frame_count;
            return Result();
        }

        Result end_frame() override
        {
            ++end_frame_count;
            return Result();
        }

        Result create_bind_group(const BindGroupDesc& desc, GpuId& out_resource_uuid) override
        {
            out_resource_uuid = next_resource++;
            bind_groups[out_resource_uuid] = desc;
            return Result();
        }

        Result create_bind_group_layout(const BindGroupLayoutDesc&, GpuId& out_resource_uuid)
            override
        {
            out_resource_uuid = next_resource++;
            return Result();
        }

        Result create_buffer(const GraphicsBufferDesc& desc, GpuId& out_resource_uuid) override
        {
            if (fail_uniform_buffer_creation && desc.usage == GraphicsBufferUsage::UNIFORM)
                return Result(false, "uniform buffer creation failed");

            out_resource_uuid = next_resource++;
            buffers[out_resource_uuid] = desc;
            return Result();
        }

        Result create_compute_pipeline(const ComputePipelineDesc& desc, GpuId& out_resource_uuid)
            override
        {
            out_resource_uuid = next_resource++;
            compute_pipelines.push_back(desc);
            return Result();
        }

        Result create_raster_pipeline(const RasterPipelineDesc& desc, GpuId& out_resource_uuid)
            override
        {
            out_resource_uuid = next_resource++;
            raster_pipelines.push_back(desc);
            return Result();
        }

        Result create_sampler(const GraphicsSamplerDesc&, GpuId& out_resource_uuid) override
        {
            out_resource_uuid = next_resource++;
            return Result();
        }

        Result create_texture(const GraphicsTextureDesc& desc, GpuId& out_resource_uuid) override
        {
            out_resource_uuid = next_resource++;
            textures.push_back(desc);
            texture_descs[out_resource_uuid] = desc;
            return Result();
        }

        Result destroy_resource(const GpuId&) override
        {
            return Result();
        }

        Result begin_render_pass(const RenderPassDesc& pass) override
        {
            current_render_pass = pass.debug_name;
            render_passes.push_back(pass.debug_name);
            return Result();
        }

        Result end_render_pass() override
        {
            current_render_pass.clear();
            return Result();
        }

        Result begin_compute_pass(const GraphicsComputePassDesc& pass) override
        {
            compute_passes.push_back(pass.debug_name);
            return Result();
        }

        Result end_compute_pass() override
        {
            return Result();
        }

        Result present() override
        {
            ++present_count;
            return Result();
        }

        void wait_for_idle() override {}

        Result bind_group(uint32, const GpuId&) override
        {
            ++bind_group_count;
            raster_index_buffer_bound = true;
            return Result();
        }

        Result bind_compute_pipeline(const GpuId&) override
        {
            ++bind_compute_count;
            return Result();
        }

        Result bind_raster_pipeline(const GpuId&) override
        {
            ++bind_raster_count;
            if (require_index_rebind_for_raster_draw)
                raster_index_buffer_bound = false;
            return Result();
        }

        Result draw(
            uint32 index_count,
            uint32 instance_count,
            uint32 first_index,
            int32 vertex_offset,
            uint32 first_instance) override
        {
            if (current_render_pass == "Toybox GBuffer Pass" && require_index_rebind_for_raster_draw
                && !raster_index_buffer_bound)
            {
                return Result(false, "index buffer not rebound after raster pipeline switch");
            }
            if (current_render_pass == "Toybox GBuffer Pass")
            {
                draw_calls.push_back(
                    DrawCallRecord {
                        .index_count = index_count,
                        .instance_count = instance_count,
                        .first_index = first_index,
                        .vertex_offset = vertex_offset,
                        .first_instance = first_instance,
                    });
            }
            return Result();
        }

        Result draw_indirect(const GpuId&, uint64, uint32 draw_count, uint32) override
        {
            indirect_draw_counts.push_back(draw_count);
            return Result();
        }

        Result draw_indirect_count(
            const GpuId&,
            uint64,
            const GpuId&,
            uint64,
            uint32 max_draw_count,
            uint32) override
        {
            indirect_draw_counts.push_back(max_draw_count);
            return Result();
        }

        Result dispatch_compute(uint32 x, uint32 y, uint32 z) override
        {
            dispatches.push_back(UVec3(x, y, z));
            return Result();
        }

        Result pipeline_barrier(const std::vector<PipelineBarrierDesc>& barriers) override
        {
            barrier_count += static_cast<uint32>(barriers.size());
            return Result();
        }

        Result write_buffer(const GpuId& resource_uuid, const void* data, uint64 data_size, uint64)
            override
        {
            const auto buffer = buffers.find(resource_uuid);
            if (fail_uniform_buffer_writes && buffer != buffers.end()
                && buffer->second.usage == GraphicsBufferUsage::UNIFORM)
            {
                return Result(false, "uniform buffer upload failed");
            }

            writes.push_back(resource_uuid);
            if (data != nullptr && data_size > 0U)
            {
                const auto* begin = static_cast<const std::byte*>(data);
                buffer_uploads[resource_uuid].assign(begin, begin + data_size);
            }
            if (data != nullptr && buffer != buffers.end()
                && buffer->second.usage == GraphicsBufferUsage::UNIFORM)
            {
                last_uniform_buffer_upload = buffer_uploads[resource_uuid];
            }
            if (data != nullptr && data_size >= sizeof(ShaderSceneUniforms))
                last_large_upload_size = data_size;
            return Result();
        }

        Result write_texture(
            const GpuId& resource_uuid,
            const GraphicsTextureUpdateDesc&,
            const void* data,
            uint64 data_size)
            override
        {
            if (fail_texture_writes)
                return Result(false, "texture upload failed");

            ++texture_write_count;
            auto bytes = std::vector<std::byte>();
            if (data != nullptr && data_size > 0U)
            {
                const auto* begin = static_cast<const std::byte*>(data);
                bytes.assign(begin, begin + data_size);
            }

            texture_uploads[resource_uuid] = std::move(bytes);
            return Result();
        }

      public:
        std::unordered_map<GpuId, GraphicsBufferDesc> buffers = {};
        std::unordered_map<GpuId, BindGroupDesc> bind_groups = {};
        std::unordered_map<GpuId, std::vector<std::byte>> buffer_uploads = {};
        std::unordered_map<GpuId, GraphicsTextureDesc> texture_descs = {};
        std::unordered_map<GpuId, std::vector<std::byte>> texture_uploads = {};
        std::vector<ComputePipelineDesc> compute_pipelines = {};
        std::vector<RasterPipelineDesc> raster_pipelines = {};
        std::vector<GraphicsTextureDesc> textures = {};
        std::vector<std::string> render_passes = {};
        std::vector<std::string> compute_passes = {};
        std::vector<UVec3> dispatches = {};
        std::vector<GpuId> writes = {};
        std::vector<DrawCallRecord> draw_calls = {};
        std::vector<uint32> indirect_draw_counts = {};
        std::vector<std::byte> last_uniform_buffer_upload = {};
        uint64 last_large_upload_size = 0U;
        uint32 begin_frame_count = 0U;
        uint32 end_frame_count = 0U;
        uint32 present_count = 0U;
        uint32 bind_group_count = 0U;
        uint32 bind_compute_count = 0U;
        uint32 bind_raster_count = 0U;
        uint32 barrier_count = 0U;
        uint32 texture_write_count = 0U;
        bool fail_texture_writes = false;
        bool require_index_rebind_for_raster_draw = false;
        bool fail_uniform_buffer_creation = false;
        bool fail_uniform_buffer_writes = false;
        bool raster_index_buffer_bound = false;
        uint32 next_resource = 1000U;
        std::string current_render_pass = {};
    };

    template <typename TService>
    static std::shared_ptr<TService> make_non_owning_service(TService& service)
    {
        return std::shared_ptr<TService>(
            &service,
            [](TService*)
            {
            });
    }

    static std::shared_ptr<SerializationRegistry> make_rendering_registry(
        std::shared_ptr<AssetLoadTracking> tracking = {})
    {
        auto registry = std::make_shared<SerializationRegistry>();
        registry->register_loader<Shader>(
            [](const std::filesystem::path& path,
               const ShaderLoadParameters&,
               const AssetLoadMetadata&,
               Shader& shader)
            {
                const auto text = path.generic_string();
                const bool is_compute = text.find(".comp") != std::string::npos;
                const bool is_fragment = text.find(".frag") != std::string::npos;
                shader = Shader(
                    text,
                    is_compute ? ShaderType::COMPUTE
                               : (is_fragment ? ShaderType::FRAGMENT : ShaderType::VERTEX));
                return Result();
            });
        registry->register_loader<Material>(
            [tracking](const std::filesystem::path& path,
               const MaterialLoadParameters&,
               const AssetLoadMetadata&,
               Material& material)
            {
                const auto text = path.generic_string();
                if (tracking)
                    tracking->material_paths.push_back(text);
                if (text.find("Broken") == std::string::npos)
                {
                    material.shader.vertex =
                        text.find("Custom") != std::string::npos
                            ? Handle("Shaders/Custom.vert")
                            : (text.find("Other") != std::string::npos
                                   ? Handle("Shaders/Other.vert")
                                   : Handle("Shaders/Pbr.vert"));
                    material.shader.fragment =
                        text.find("Custom") != std::string::npos
                            ? Handle("Shaders/Custom.frag")
                            : (text.find("Other") != std::string::npos
                                   ? Handle("Shaders/Other.frag")
                                   : Handle("Shaders/Pbr.frag"));
                }
                material.config.is_cullable = text.find("Custom") == std::string::npos;
                material.config.is_two_sided = text.find("Custom") != std::string::npos;
                const auto base_color =
                    text.find("Magenta") != std::string::npos ? Color::MAGENTA : Color::WHITE;
                const auto emissive_color =
                    text.find("Magenta") != std::string::npos ? Color::MAGENTA : Color::BLACK;
                material.parameters.set(
                    make_material_parameter(
                        "albedo_color",
                        base_color,
                        MaterialBindingTarget::BASE_COLOR));
                material.parameters.set(
                    make_material_parameter(
                        "emissive_color",
                        emissive_color,
                        MaterialBindingTarget::EMISSIVE_COLOR));
                material.parameters.set(
                    make_material_parameter(
                        "metallic",
                        0.0F,
                        MaterialBindingTarget::METALLIC));
                material.parameters.set(
                    make_material_parameter(
                        "roughness",
                        1.0F,
                        MaterialBindingTarget::ROUGHNESS));
                material.parameters.set(
                    make_material_parameter(
                        "normal_strength",
                        1.0F,
                        MaterialBindingTarget::NORMAL_STRENGTH));
                material.parameters.set(
                    make_material_parameter("ao", 1.0F, MaterialBindingTarget::AO));
                material.parameters.set(
                    make_material_parameter(
                        "alpha_cutoff",
                        0.0F,
                        MaterialBindingTarget::ALPHA_CUTOFF));
                material.textures.set(
                    make_material_texture_binding(
                        "albedo_map",
                        text.find("MissingTextured") != std::string::npos
                            ? Handle("Textures/DoesNotExist.tex")
                            : (text.find("Textured") != std::string::npos
                                   ? Handle("Textures/Checker.tex")
                                   : Handle()),
                        MaterialBindingTarget::ALBEDO_TEXTURE));
                material.textures.set(
                    make_material_texture_binding(
                        "normal_map",
                        {},
                        MaterialBindingTarget::NORMAL_TEXTURE));
                material.textures.set(
                    make_material_texture_binding(
                        "metallic_map",
                        {},
                        MaterialBindingTarget::METALLIC_TEXTURE));
                material.textures.set(
                    make_material_texture_binding(
                        "roughness_map",
                        {},
                        MaterialBindingTarget::ROUGHNESS_TEXTURE));
                material.textures.set(
                    make_material_texture_binding(
                        "ao_map",
                        {},
                        MaterialBindingTarget::AO_TEXTURE));
                material.textures.set(
                    make_material_texture_binding(
                        "emissive_map",
                        {},
                        MaterialBindingTarget::EMISSIVE_TEXTURE));
                return Result();
            });
        registry->register_loader<Texture>(
            [](const std::filesystem::path& path,
               const TextureLoadParameters&,
               const AssetLoadMetadata&,
               Texture& texture)
            {
                const auto text = path.generic_string();
                if (text.find("DoesNotExist") != std::string::npos)
                    return Result(false, "Missing test texture.");

                texture = Texture(
                    Size {.width = 1U, .height = 1U},
                    TextureWrap::REPEAT,
                    TextureFilter::LINEAR,
                    TextureFormat::RGBA,
                    std::vector<Pixel> {255U, 0U, 0U, 255U});
                return Result();
            });
        registry->register_loader<Model>(
            [tracking](const std::filesystem::path& path,
               const ModelLoadParameters&,
               const AssetLoadMetadata&,
               Model& model)
            {
                if (tracking)
                    tracking->model_paths.push_back(path.generic_string());
                model = Model(Mesh::TRIANGLE);
                return Result();
            });
        return registry;
    }

    static std::optional<std::reference_wrapper<const std::vector<std::byte>>> find_uploaded_texture(
        const RecordingGraphicsBackend& backend,
        const std::string_view debug_name)
    {
        for (const auto& [resource, desc] : backend.texture_descs)
        {
            if (desc.debug_name != debug_name)
                continue;

            const auto upload = backend.texture_uploads.find(resource);
            if (upload == backend.texture_uploads.end())
                return std::nullopt;

            return std::cref(upload->second);
        }

        return std::nullopt;
    }

    static const ShaderSceneUniforms* try_get_uploaded_scene_uniforms(
        const RecordingGraphicsBackend& backend)
    {
        if (backend.last_uniform_buffer_upload.size() < sizeof(ShaderSceneUniforms))
            return nullptr;

        return reinterpret_cast<const ShaderSceneUniforms*>(
            backend.last_uniform_buffer_upload.data());
    }

    static const ShaderLightData* try_get_uploaded_light_buffer(
        const RecordingGraphicsBackend& backend,
        uint32& out_light_count)
    {
        for (const auto& [resource, desc] : backend.buffers)
        {
            if (desc.debug_name != "global_lights")
                continue;

            const auto upload = backend.buffer_uploads.find(resource);
            if (upload == backend.buffer_uploads.end()
                || upload->second.size() < sizeof(ShaderLightData))
            {
                return nullptr;
            }

            out_light_count =
                static_cast<uint32>(upload->second.size() / sizeof(ShaderLightData));
            return reinterpret_cast<const ShaderLightData*>(upload->second.data());
        }

        return nullptr;
    }

    static const ShaderInstanceData* try_get_uploaded_instance_buffer(
        const RecordingGraphicsBackend& backend,
        uint32& out_instance_count)
    {
        for (const auto& [resource, desc] : backend.buffers)
        {
            if (desc.debug_name != "all_instances")
                continue;

            const auto upload = backend.buffer_uploads.find(resource);
            if (upload == backend.buffer_uploads.end()
                || upload->second.size() < sizeof(ShaderInstanceData))
            {
                return nullptr;
            }

            out_instance_count =
                static_cast<uint32>(upload->second.size() / sizeof(ShaderInstanceData));
            return reinterpret_cast<const ShaderInstanceData*>(upload->second.data());
        }

        return nullptr;
    }

    static const ShaderMaterialData* try_get_uploaded_material_buffer(
        const RecordingGraphicsBackend& backend,
        uint32& out_material_count)
    {
        for (const auto& [resource, desc] : backend.buffers)
        {
            if (desc.debug_name != "global_materials")
                continue;

            const auto upload = backend.buffer_uploads.find(resource);
            if (upload == backend.buffer_uploads.end()
                || upload->second.size() < sizeof(ShaderMaterialData))
            {
                return nullptr;
            }

            out_material_count =
                static_cast<uint32>(upload->second.size() / sizeof(ShaderMaterialData));
            return reinterpret_cast<const ShaderMaterialData*>(upload->second.data());
        }

        return nullptr;
    }

    static bool has_created_buffer(
        const RecordingGraphicsBackend& backend,
        const std::string_view debug_name)
    {
        for (const auto& [_, desc] : backend.buffers)
        {
            if (desc.debug_name == debug_name)
                return true;
        }

        return false;
    }

    static std::optional<std::reference_wrapper<const GraphicsTextureDesc>> find_created_texture(
        const RecordingGraphicsBackend& backend,
        const std::string_view debug_name)
    {
        for (const auto& [_, desc] : backend.texture_descs)
        {
            if (desc.debug_name == debug_name)
                return std::cref(desc);
        }

        return std::nullopt;
    }

    static uint32 count_buffer_writes(
        const RecordingGraphicsBackend& backend,
        const std::string_view debug_name)
    {
        auto write_count = uint32(0U);
        for (const auto resource : backend.writes)
        {
            const auto buffer = backend.buffers.find(resource);
            if (buffer == backend.buffers.end() || buffer->second.debug_name != debug_name)
                continue;

            ++write_count;
        }

        return write_count;
    }

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
        const auto* uniforms = try_get_uploaded_scene_uniforms(backend);
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
        const auto* uniforms = try_get_uploaded_scene_uniforms(backend);
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

    TEST(RenderingPipelineTests, Execute_SucceedsWhenSceneUniformUploadSucceeds)
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

    TEST(RenderingPipelineTests, Execute_ChildCameraUsesParentWorldTransformForSceneUniforms)
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
        const auto* uniforms = try_get_uploaded_scene_uniforms(backend);

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(uniforms, nullptr);
        EXPECT_NEAR(uniforms->camera_position_time.x, 3.585786F, 0.001F);
        EXPECT_NEAR(uniforms->camera_position_time.y, 3.5F, 0.001F);
        EXPECT_NEAR(uniforms->camera_position_time.z, 7.585786F, 0.001F);
    }

    TEST(RenderingPipelineTests, Execute_CameraWithoutParentTransformFallsBackToLocalSceneUniforms)
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
        const auto* uniforms = try_get_uploaded_scene_uniforms(backend);

        // Assert
        ASSERT_TRUE(result.succeeded());
        ASSERT_NE(uniforms, nullptr);
        EXPECT_FLOAT_EQ(uniforms->camera_position_time.x, 1.0F);
        EXPECT_FLOAT_EQ(uniforms->camera_position_time.y, 2.0F);
        EXPECT_FLOAT_EQ(uniforms->camera_position_time.z, 3.0F);
    }

    TEST(RenderingPipelineTests, Execute_FailsWhenSceneUniformUploadFails)
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
