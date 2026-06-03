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
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/transform.h"
#include "gtest/gtest.h"
#include <future>
#include <unordered_map>

namespace tbx::tests::graphics
{
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

        Result create_bind_group(const BindGroupDesc& desc, Uuid& out_resource_uuid) override
        {
            out_resource_uuid = Uuid(next_resource++);
            bind_groups[out_resource_uuid] = desc;
            return Result();
        }

        Result create_bind_group_layout(const BindGroupLayoutDesc&, Uuid& out_resource_uuid)
            override
        {
            out_resource_uuid = Uuid(next_resource++);
            return Result();
        }

        Result create_buffer(const GraphicsBufferDesc& desc, Uuid& out_resource_uuid) override
        {
            out_resource_uuid = Uuid(next_resource++);
            buffers[out_resource_uuid] = desc;
            return Result();
        }

        Result create_compute_pipeline(const ComputePipelineDesc& desc, Uuid& out_resource_uuid)
            override
        {
            out_resource_uuid = Uuid(next_resource++);
            compute_pipelines.push_back(desc);
            return Result();
        }

        Result create_raster_pipeline(const RasterPipelineDesc& desc, Uuid& out_resource_uuid)
            override
        {
            out_resource_uuid = Uuid(next_resource++);
            raster_pipelines.push_back(desc);
            return Result();
        }

        Result create_sampler(const GraphicsSamplerDesc&, Uuid& out_resource_uuid) override
        {
            out_resource_uuid = Uuid(next_resource++);
            return Result();
        }

        Result create_texture(const GraphicsTextureDesc& desc, Uuid& out_resource_uuid) override
        {
            out_resource_uuid = Uuid(next_resource++);
            textures.push_back(desc);
            return Result();
        }

        Result destroy_resource(const Uuid&) override
        {
            return Result();
        }

        Result begin_render_pass(const GraphicsRenderPassDesc& pass) override
        {
            render_passes.push_back(pass.debug_name);
            return Result();
        }

        Result end_render_pass() override
        {
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

        Result bind_group(uint32, const Uuid&) override
        {
            ++bind_group_count;
            return Result();
        }

        Result bind_compute_pipeline(const Uuid&) override
        {
            ++bind_compute_count;
            return Result();
        }

        Result bind_raster_pipeline(const Uuid&) override
        {
            ++bind_raster_count;
            return Result();
        }

        Result draw(uint32, uint32, uint32, int32, uint32) override
        {
            return Result();
        }

        Result draw_indirect(const Uuid&, uint64, uint32 draw_count, uint32) override
        {
            indirect_draw_counts.push_back(draw_count);
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

        Result write_buffer(const Uuid& resource_uuid, const void* data, uint64 data_size, uint64)
            override
        {
            writes.push_back(resource_uuid);
            if (data != nullptr && data_size >= sizeof(ShaderSceneUniforms))
                last_large_upload_size = data_size;
            return Result();
        }

        Result write_texture(const Uuid&, const GraphicsTextureUpdateDesc&, const void*, uint64)
            override
        {
            ++texture_write_count;
            return Result();
        }

      public:
        std::unordered_map<Uuid, GraphicsBufferDesc> buffers = {};
        std::unordered_map<Uuid, BindGroupDesc> bind_groups = {};
        std::vector<ComputePipelineDesc> compute_pipelines = {};
        std::vector<RasterPipelineDesc> raster_pipelines = {};
        std::vector<GraphicsTextureDesc> textures = {};
        std::vector<std::string> render_passes = {};
        std::vector<std::string> compute_passes = {};
        std::vector<UVec3> dispatches = {};
        std::vector<Uuid> writes = {};
        std::vector<uint32> indirect_draw_counts = {};
        uint64 last_large_upload_size = 0U;
        uint32 begin_frame_count = 0U;
        uint32 end_frame_count = 0U;
        uint32 present_count = 0U;
        uint32 bind_group_count = 0U;
        uint32 bind_compute_count = 0U;
        uint32 bind_raster_count = 0U;
        uint32 barrier_count = 0U;
        uint32 texture_write_count = 0U;
        uint32 next_resource = 1000U;
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

    static std::shared_ptr<SerializationRegistry> make_rendering_registry()
    {
        auto registry = std::make_shared<SerializationRegistry>();
        registry->register_loader<Shader>(
            [](const std::filesystem::path& path,
               const ShaderLoadParameters&,
               const AssetLoadMetadata&,
               Shader& shader)
            {
                const auto text = path.generic_string();
                const bool is_compute = text.find("GpuCulling") != std::string::npos;
                const bool is_fragment = text.find(".frag") != std::string::npos;
                shader = Shader(
                    "#version 460 core\nvoid main() {}\n",
                    is_compute ? ShaderType::COMPUTE
                               : (is_fragment ? ShaderType::FRAGMENT : ShaderType::VERTEX));
                return Result();
            });
        registry->register_loader<Material>(
            [](const std::filesystem::path& path,
               const MaterialLoadParameters&,
               const AssetLoadMetadata&,
               Material& material)
            {
                const auto text = path.generic_string();
                if (text.find("Pipeline") != std::string::npos)
                {
                    material.shader.computes = {Handle("Shaders/GpuCulling.comp")};
                    return Result();
                }

                material.shader.vertex = Handle("Shaders/Pbr.vert");
                material.shader.fragment = Handle("Shaders/Pbr.frag");
                material.parameters.set("albedo_color", Color::WHITE);
                return Result();
            });
        registry->register_loader<Model>(
            [](const std::filesystem::path&,
               const ModelLoadParameters&,
               const AssetLoadMetadata&,
               Model& model)
            {
                model = Model(Mesh::TRIANGLE);
                return Result();
            });
        return registry;
    }

    TEST(RenderingPipelineTests, Execute_WithRenderableWorldSubmitsGpuComputeAndIndirectDraws)
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
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            backend_service,
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
        EXPECT_FALSE(backend.indirect_draw_counts.empty());
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
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto world_manager_service = make_non_owning_service(world_manager);
        auto pipeline = RenderingPipeline(
            backend_service,
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
        EXPECT_TRUE(backend.indirect_draw_counts.empty());
    }
}
