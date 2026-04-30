#include "PCH.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/material.h"
#include "tbx/systems/graphics/mesh.h"
#include "tbx/systems/graphics/model.h"
#include "tbx/systems/graphics/render_pipeline.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/shader.h"
#include "tbx/systems/graphics/texture.h"
#include "tbx/systems/math/transform.h"
#include <filesystem>
#include <future>
#include <memory>
#include <utility>
#include <vector>

namespace tbx::tests::graphics
{
    enum class GraphicsBackendCallback
    {
        BeginFrame,
        BeginView,
        SetViewport,
        BeginPass,
        EndPass,
        BindPipeline,
        BindVertexBuffer,
        BindIndexBuffer,
        BindUniformBuffer,
        BindStorageBuffer,
        BindTexture,
        BindSampler,
        Draw,
        DrawIndexed,
        EndView,
        Present,
        EndFrame,
        WaitForIdle,
    };

    class RecordingGraphicsBackend final : public IGraphicsBackend
    {
      public:
        Result begin_frame(const GraphicsFrameInfo& frame) override
        {
            recorded_output_window = frame.output_window;
            recorded_render_resolution = frame.render_resolution;
            callbacks.push_back(GraphicsBackendCallback::BeginFrame);
            return {};
        }

        Result begin_pass(const GraphicsPassDesc& pass) override
        {
            recorded_pass = pass;
            callbacks.push_back(GraphicsBackendCallback::BeginPass);
            return {};
        }

        Result begin_view(const GraphicsView& view) override
        {
            recorded_viewport = view.viewport.dimensions;
            callbacks.push_back(GraphicsBackendCallback::BeginView);
            return {};
        }

        Result bind_index_buffer(const Uuid& buffer_resource_uuid, GraphicsIndexType index_type)
            override
        {
            recorded_index_buffer = buffer_resource_uuid;
            recorded_index_type = index_type;
            callbacks.push_back(GraphicsBackendCallback::BindIndexBuffer);
            return {};
        }

        Result bind_pipeline(const Uuid& pipeline_resource_uuid) override
        {
            recorded_pipeline = pipeline_resource_uuid;
            callbacks.push_back(GraphicsBackendCallback::BindPipeline);
            return {};
        }

        Result bind_sampler(uint32 slot, const Uuid& sampler_resource_uuid) override
        {
            recorded_sampler_slot = slot;
            recorded_sampler = sampler_resource_uuid;
            callbacks.push_back(GraphicsBackendCallback::BindSampler);
            return {};
        }

        Result bind_storage_buffer(uint32, const Uuid&) override
        {
            callbacks.push_back(GraphicsBackendCallback::BindStorageBuffer);
            return {};
        }

        Result bind_texture(uint32 slot, const Uuid& texture_resource_uuid) override
        {
            recorded_texture_slot = slot;
            recorded_texture = texture_resource_uuid;
            callbacks.push_back(GraphicsBackendCallback::BindTexture);
            return {};
        }

        Result bind_uniform_buffer(uint32 slot, const Uuid& buffer_resource_uuid) override
        {
            recorded_uniform_slot = slot;
            recorded_uniform_buffer = buffer_resource_uuid;
            callbacks.push_back(GraphicsBackendCallback::BindUniformBuffer);
            return {};
        }

        Result bind_vertex_buffer(uint32 slot, const Uuid& buffer_resource_uuid) override
        {
            recorded_vertex_slot = slot;
            recorded_vertex_buffer = buffer_resource_uuid;
            callbacks.push_back(GraphicsBackendCallback::BindVertexBuffer);
            return {};
        }

        Result draw(uint32, uint32) override
        {
            callbacks.push_back(GraphicsBackendCallback::Draw);
            return {};
        }

        Result draw_indexed(const GraphicsDrawIndexedDesc& draw) override
        {
            recorded_draw = draw;
            callbacks.push_back(GraphicsBackendCallback::DrawIndexed);
            return {};
        }

        Result end_frame() override
        {
            callbacks.push_back(GraphicsBackendCallback::EndFrame);
            return {};
        }

        Result end_pass() override
        {
            callbacks.push_back(GraphicsBackendCallback::EndPass);
            return {};
        }

        Result end_view() override
        {
            callbacks.push_back(GraphicsBackendCallback::EndView);
            return {};
        }

        GraphicsApi get_api() const override
        {
            return GraphicsApi::OPEN_GL;
        }

        Result initialize(const GraphicsSettings&) override
        {
            return {};
        }

        Result present() override
        {
            callbacks.push_back(GraphicsBackendCallback::Present);
            return {};
        }

        Result set_scissor(const Viewport& scissor) override
        {
            recorded_scissor = scissor.dimensions;
            return {};
        }

        Result set_viewport(const Viewport& viewport) override
        {
            recorded_viewport = viewport.dimensions;
            callbacks.push_back(GraphicsBackendCallback::SetViewport);
            return {};
        }

        void shutdown() override {}

        Result unload(const Uuid& resource_uuid) override
        {
            unloaded_resources.push_back(resource_uuid);
            return {};
        }

        Result update_buffer(const Uuid&, const void*, uint64, uint64) override
        {
            return {};
        }

        Result update_settings(const GraphicsSettings&) override
        {
            return {};
        }

        Result update_texture(const Uuid&, const GraphicsTextureUpdateDesc&, const void*, uint64)
            override
        {
            return {};
        }

        Result upload_buffer(
            const GraphicsBufferDesc& desc,
            const void*,
            uint64,
            Uuid& out_resource_uuid) override
        {
            recorded_buffer_descs.push_back(desc);
            uploaded_buffer_count += 1U;
            out_resource_uuid = Uuid(next_uploaded_resource++);
            return {};
        }

        Result upload_pipeline(const GraphicsPipelineDesc& desc, Uuid& out_resource_uuid) override
        {
            recorded_pipeline_desc = desc;
            uploaded_pipeline_count += 1U;
            out_resource_uuid = Uuid(next_uploaded_resource++);
            return {};
        }

        Result upload_sampler(const GraphicsSamplerDesc&, Uuid&) override
        {
            return {};
        }

        Result upload_texture(
            const GraphicsTextureDesc& desc,
            const void*,
            uint64 data_size,
            Uuid& out_resource_uuid) override
        {
            recorded_texture_desc = desc;
            recorded_texture_upload_size = data_size;
            uploaded_texture_count += 1U;
            out_resource_uuid = Uuid(next_uploaded_resource++);
            return {};
        }

        void wait_for_idle() override
        {
            callbacks.push_back(GraphicsBackendCallback::WaitForIdle);
        }

      public:
        std::vector<GraphicsBackendCallback> callbacks = {};
        Window recorded_output_window = {};
        Size recorded_render_resolution = {};
        Size recorded_viewport = {};
        Size recorded_scissor = {};
        GraphicsPassDesc recorded_pass = {};
        Uuid recorded_pipeline = {};
        Uuid recorded_vertex_buffer = {};
        Uuid recorded_index_buffer = {};
        Uuid recorded_uniform_buffer = {};
        Uuid recorded_texture = {};
        Uuid recorded_sampler = {};
        uint32 recorded_vertex_slot = 0U;
        uint32 recorded_uniform_slot = 0U;
        uint32 recorded_texture_slot = 0U;
        uint32 recorded_sampler_slot = 0U;
        GraphicsIndexType recorded_index_type = GraphicsIndexType::UINT32;
        GraphicsDrawIndexedDesc recorded_draw = {};
        GraphicsPipelineDesc recorded_pipeline_desc = {};
        std::vector<GraphicsBufferDesc> recorded_buffer_descs = {};
        GraphicsTextureDesc recorded_texture_desc = {};
        std::vector<Uuid> unloaded_resources = {};
        uint64 recorded_texture_upload_size = 0U;
        uint uploaded_buffer_count = 0U;
        uint uploaded_pipeline_count = 0U;
        uint uploaded_texture_count = 0U;
        uint32 next_uploaded_resource = 1000U;
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

    class RecordingWindowManager final : public IWindowManager
    {
      public:
        Window create(const WindowCreateInfo& create_info = {}) override
        {
            (void)create_info;
            return window;
        }

        bool destroy(const Window& target_window) override
        {
            if (target_window != window)
                return false;

            is_window_open = false;
            return true;
        }

        bool has(const Window& target_window) const override
        {
            return target_window == window;
        }

        bool open(const Window& target_window) override
        {
            if (target_window != window)
                return false;

            is_window_open = true;
            return true;
        }

        bool close(const Window& target_window) override
        {
            if (target_window != window)
                return false;

            is_window_open = false;
            return true;
        }

        bool is_open(const Window& target_window) const override
        {
            return target_window == window && is_window_open;
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
            return "main";
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
            return size;
        }

        bool set_size(const Window&, const Size& next_size) override
        {
            size = next_size;
            return true;
        }

        std::vector<Window> get_open_windows() const override
        {
            return is_window_open ? std::vector<Window> {window} : std::vector<Window> {};
        }

      public:
        Window window = Window("main");
        Size size = Size {1280U, 720U};
        bool is_window_open = true;
    };

    // Validates Rendering opens frame state and submits geometry through render().
    TEST(RenderingTests, Render_DelegatesFrameAndGeometryPassCommands)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto registry = EntityRegistry {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = NullMessageDispatcher {};
        auto serialization_registry = SerializationRegistry {};
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto settings =
            GraphicsSettings(dispatcher, false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto entity = Entity("Triangle", registry);
        entity.add_component<DynamicMesh>(triangle);
        entity.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));

        // Act
        auto rendering = Rendering(
            backend,
            registry,
            asset_manager,
            window_manager,
            window_manager.window,
            settings);
        const auto result = rendering.render();

        // Assert
        const auto expected_callbacks = std::vector<GraphicsBackendCallback> {
            GraphicsBackendCallback::BeginFrame,
            GraphicsBackendCallback::BeginView,
            GraphicsBackendCallback::SetViewport,
            GraphicsBackendCallback::BeginPass,
            GraphicsBackendCallback::BindPipeline,
            GraphicsBackendCallback::BindVertexBuffer,
            GraphicsBackendCallback::BindIndexBuffer,
            GraphicsBackendCallback::DrawIndexed,
            GraphicsBackendCallback::EndPass,
            GraphicsBackendCallback::EndView,
            GraphicsBackendCallback::Present,
            GraphicsBackendCallback::EndFrame,
        };

        EXPECT_TRUE(result);
        EXPECT_EQ(backend.recorded_output_window.get_id(), window_manager.window.get_id());
        EXPECT_EQ(backend.recorded_render_resolution.width, window_manager.size.width);
        EXPECT_EQ(backend.recorded_render_resolution.height, window_manager.size.height);
        EXPECT_EQ(backend.recorded_viewport.width, window_manager.size.width);
        EXPECT_EQ(backend.recorded_viewport.height, window_manager.size.height);
        EXPECT_EQ(backend.recorded_pass.clear_flags, GraphicsClearFlags::COLOR_DEPTH);
        EXPECT_EQ(backend.recorded_pass.debug_name, "Toybox Geometry Pass");
        EXPECT_EQ(backend.callbacks, expected_callbacks);
    }

    // Validates Toybox pass code can own draw behavior with explicit backend commands.
    TEST(GraphicsBackendTests, ExplicitCommands_CanDescribeIndexedGeometryDraw)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        const auto pipeline = Uuid(10U);
        const auto vertex_buffer = Uuid(20U);
        const auto index_buffer = Uuid(30U);
        const auto uniform_buffer = Uuid(40U);
        const auto texture = Uuid(50U);
        const auto sampler = Uuid(60U);
        const auto draw = GraphicsDrawIndexedDesc {
            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
            .index_type = GraphicsIndexType::UINT32,
            .index_count = 36U,
            .index_offset = 0U,
            .vertex_offset = 0,
            .instance_count = 1U,
            .first_instance = 0U,
        };

        // Act
        auto result = backend.bind_pipeline(pipeline);
        if (result)
            result = backend.bind_vertex_buffer(0U, vertex_buffer);
        if (result)
            result = backend.bind_index_buffer(index_buffer, GraphicsIndexType::UINT32);
        if (result)
            result = backend.bind_uniform_buffer(0U, uniform_buffer);
        if (result)
            result = backend.bind_texture(1U, texture);
        if (result)
            result = backend.bind_sampler(1U, sampler);
        if (result)
            result = backend.draw_indexed(draw);

        // Assert
        const auto expected_callbacks = std::vector<GraphicsBackendCallback> {
            GraphicsBackendCallback::BindPipeline,
            GraphicsBackendCallback::BindVertexBuffer,
            GraphicsBackendCallback::BindIndexBuffer,
            GraphicsBackendCallback::BindUniformBuffer,
            GraphicsBackendCallback::BindTexture,
            GraphicsBackendCallback::BindSampler,
            GraphicsBackendCallback::DrawIndexed,
        };

        EXPECT_TRUE(result);
        EXPECT_EQ(backend.recorded_pipeline, pipeline);
        EXPECT_EQ(backend.recorded_vertex_buffer, vertex_buffer);
        EXPECT_EQ(backend.recorded_index_buffer, index_buffer);
        EXPECT_EQ(backend.recorded_uniform_buffer, uniform_buffer);
        EXPECT_EQ(backend.recorded_texture, texture);
        EXPECT_EQ(backend.recorded_sampler, sampler);
        EXPECT_EQ(backend.recorded_texture_slot, 1U);
        EXPECT_EQ(backend.recorded_sampler_slot, 1U);
        EXPECT_EQ(backend.recorded_draw.index_count, 36U);
        EXPECT_EQ(backend.callbacks, expected_callbacks);
    }

    // Validates Toybox-owned render pipelines execute passes through backend commands.
    TEST(GraphicsRenderPipelineTests, Execute_SubmitsIndexedDrawPassCommands)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto pipeline = GraphicsRenderPipeline {backend};
        pipeline.add_pass_operation(
            GraphicsRenderPass {
                .pass =
                    GraphicsPassDesc {
                        .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                        .debug_name = "Geometry",
                    },
                .viewport =
                    Viewport {
                        .position = Vec2(0.0F),
                        .dimensions = Size {320U, 200U},
                    },
                .indexed_draws =
                    {
                        GraphicsIndexedDrawCommand {
                            .pipeline = Uuid(1U),
                            .vertex_buffers = {GraphicsResourceBinding {
                                .slot = 0U,
                                .resource = Uuid(2U),
                            }},
                            .index_buffer = Uuid(3U),
                            .index_type = GraphicsIndexType::UINT32,
                            .uniform_buffers = {GraphicsResourceBinding {
                                .slot = 0U,
                                .resource = Uuid(4U),
                            }},
                            .textures = {GraphicsResourceBinding {
                                .slot = 1U,
                                .resource = Uuid(5U),
                            }},
                            .samplers = {GraphicsResourceBinding {
                                .slot = 1U,
                                .resource = Uuid(6U),
                            }},
                            .draw =
                                GraphicsDrawIndexedDesc {
                                    .index_count = 6U,
                                },
                        },
                    },
            });

        // Act
        const auto result = pipeline.execute();

        // Assert
        const auto expected_callbacks = std::vector<GraphicsBackendCallback> {
            GraphicsBackendCallback::SetViewport,
            GraphicsBackendCallback::BeginPass,
            GraphicsBackendCallback::BindPipeline,
            GraphicsBackendCallback::BindVertexBuffer,
            GraphicsBackendCallback::BindIndexBuffer,
            GraphicsBackendCallback::BindUniformBuffer,
            GraphicsBackendCallback::BindTexture,
            GraphicsBackendCallback::BindSampler,
            GraphicsBackendCallback::DrawIndexed,
            GraphicsBackendCallback::EndPass,
        };

        EXPECT_TRUE(result);
        EXPECT_EQ(backend.callbacks, expected_callbacks);
    }

    // Validates asset-backed pipeline commands resolve materials and textures through the cache.
    TEST(GraphicsRenderPipelineTests, Execute_ResolvesAssetBackedDrawResources)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto dispatcher = NullMessageDispatcher {};
        auto serialization_registry = SerializationRegistry {};
        serialization_registry.register_reader<Shader>(
            [](const std::filesystem::path&, const ShaderLoadParameters&)
            {
                return std::make_shared<Shader>(std::vector<ShaderSource> {
                    ShaderSource(
                        "#version 450 core\nvoid main(){ gl_Position = vec4(0.0); }\n",
                        ShaderType::VERTEX),
                    ShaderSource(
                        "#version 450 core\nlayout(location=0) out vec4 c; void main(){ c = "
                        "vec4(1.0); }\n",
                        ShaderType::FRAGMENT),
                });
            });
        serialization_registry.register_reader<Texture>(
            [](const std::filesystem::path&, const TextureLoadParameters&)
            {
                return std::make_shared<Texture>(
                    Size {1U, 1U},
                    TextureWrap::REPEAT,
                    TextureFilter::LINEAR,
                    TextureFormat::RGBA,
                    std::vector<Pixel> {255U, 255U, 255U, 255U});
            });
        serialization_registry.register_reader<Material>(
            [](const std::filesystem::path&, const MaterialLoadParameters&)
            {
                auto material = Material {};
                material.program.vertex = Handle("Shaders/Asset.shader");
                material.program.fragment = Handle("Shaders/Asset.shader");
                material.textures.set("diffuse_map", Handle("Textures/Asset.png"));
                return std::make_shared<Material>(std::move(material));
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto resource_manager = GraphicsResourceManager(backend, asset_manager, 3U);
        auto pipeline = GraphicsRenderPipeline {backend, resource_manager};
        pipeline.add_pass_operation(
            GraphicsRenderPass {
                .pass =
                    GraphicsPassDesc {
                        .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                        .debug_name = "AssetBacked",
                    },
                .indexed_draws =
                    {
                        GraphicsIndexedDrawCommand {
                            .material = Handle("Materials/Asset.mat"),
                            .vertex_buffers = {GraphicsResourceBinding {
                                .slot = 0U,
                                .resource = Uuid(20U),
                            }},
                            .index_buffer = Uuid(30U),
                            .texture_assets = {GraphicsAssetResourceBinding {
                                .slot = 1U,
                                .asset = Handle("Textures/Asset.png"),
                            }},
                            .draw =
                                GraphicsDrawIndexedDesc {
                                    .index_count = 3U,
                                },
                        },
                    },
            });

        // Act
        const auto result = pipeline.execute();

        // Assert
        const auto expected_callbacks = std::vector<GraphicsBackendCallback> {
            GraphicsBackendCallback::BeginPass,
            GraphicsBackendCallback::BindPipeline,
            GraphicsBackendCallback::BindVertexBuffer,
            GraphicsBackendCallback::BindIndexBuffer,
            GraphicsBackendCallback::BindTexture,
            GraphicsBackendCallback::DrawIndexed,
            GraphicsBackendCallback::EndPass,
        };

        EXPECT_TRUE(result);
        EXPECT_EQ(backend.callbacks, expected_callbacks);
        EXPECT_EQ(backend.uploaded_pipeline_count, 1U);
        EXPECT_EQ(backend.uploaded_texture_count, 1U);
        EXPECT_EQ(backend.recorded_pipeline, Uuid(1001U));
        EXPECT_EQ(backend.recorded_texture, Uuid(1000U));
    }

    // Validates Rendering owns the Toybox geometry pass and submits render() commands.
    TEST(GraphicsRenderPipelineTests, Rendering_RenderSubmitsGeometryPass)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto registry = EntityRegistry {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = NullMessageDispatcher {};
        auto serialization_registry = SerializationRegistry {};
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto settings =
            GraphicsSettings(dispatcher, false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto entity = Entity("Cube", registry);
        entity.add_component<DynamicMesh>(cube);
        entity.add_component<Transform>(Vec3(0.0F, 0.0F, -4.0F));

        // Act
        auto rendering = Rendering(
            backend,
            registry,
            asset_manager,
            window_manager,
            window_manager.window,
            settings);
        const auto render_result = rendering.render();

        // Assert
        const auto expected_callbacks = std::vector<GraphicsBackendCallback> {
            GraphicsBackendCallback::BeginFrame,
            GraphicsBackendCallback::BeginView,
            GraphicsBackendCallback::SetViewport,
            GraphicsBackendCallback::BeginPass,
            GraphicsBackendCallback::BindPipeline,
            GraphicsBackendCallback::BindVertexBuffer,
            GraphicsBackendCallback::BindIndexBuffer,
            GraphicsBackendCallback::DrawIndexed,
            GraphicsBackendCallback::EndPass,
            GraphicsBackendCallback::EndView,
            GraphicsBackendCallback::Present,
            GraphicsBackendCallback::EndFrame,
        };

        EXPECT_TRUE(render_result);
        EXPECT_EQ(backend.recorded_pass.debug_name, "Toybox Geometry Pass");
        EXPECT_EQ(backend.recorded_pass.clear_flags, GraphicsClearFlags::COLOR_DEPTH);
        EXPECT_EQ(backend.callbacks, expected_callbacks);
    }

    // Validates static mesh rendering uses cached model buffers without reloading CPU assets.
    TEST(RenderingTests, Render_StaticMeshUsesCachedModelResourceWithoutReloadingAsset)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto registry = EntityRegistry {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = NullMessageDispatcher {};
        auto serialization_registry = SerializationRegistry {};
        auto model_load_count = uint {};
        serialization_registry.register_reader<Model>(
            [&model_load_count](const std::filesystem::path&, const ModelLoadParameters&)
            {
                model_load_count += 1U;
                return std::make_shared<Model>(triangle);
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto settings =
            GraphicsSettings(dispatcher, false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        const auto model_handle = Handle("Models/Triangle.fbx");
        auto entity = Entity("StaticTriangle", registry);
        entity.add_component<StaticMesh>(StaticMesh {.handle = model_handle});
        entity.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto rendering = Rendering(
            backend,
            registry,
            asset_manager,
            window_manager,
            window_manager.window,
            settings);

        // Act
        const auto first_result = rendering.render();
        asset_manager.unload_unreferenced();
        const AssetUsage usage_after_asset_cleanup = asset_manager.get_usage<Model>(model_handle);
        const auto second_result = rendering.render();

        // Assert
        EXPECT_TRUE(first_result);
        EXPECT_TRUE(second_result);
        EXPECT_EQ(model_load_count, 1U);
        EXPECT_EQ(usage_after_asset_cleanup.stream_state, AssetStreamState::UNLOADED);
        EXPECT_EQ(backend.uploaded_buffer_count, 3U);
        EXPECT_EQ(backend.uploaded_pipeline_count, 2U);
        EXPECT_EQ(backend.recorded_draw.index_count, 3U);
    }

    // Validates GraphicsResourceManager reuses an uploaded texture resource for repeated handles.
    TEST(GraphicsResourceManagerTests, LoadTexture_CachesUploadedResource)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto dispatcher = NullMessageDispatcher {};
        auto serialization_registry = SerializationRegistry {};
        serialization_registry.register_reader<Texture>(
            [](const std::filesystem::path&, const TextureLoadParameters&)
            {
                return std::make_shared<Texture>(
                    Size {2U, 2U},
                    TextureWrap::REPEAT,
                    TextureFilter::LINEAR,
                    TextureFormat::RGB,
                    std::vector<Pixel> {
                        255U,
                        0U,
                        0U,
                        0U,
                        255U,
                        0U,
                        0U,
                        0U,
                        255U,
                        255U,
                        255U,
                        255U,
                    });
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto resource_manager = GraphicsResourceManager(backend, asset_manager, 3U);
        const auto texture_handle = Handle("Textures/Diffuse.png");

        // Act
        auto first_resource = Uuid {};
        auto second_resource = Uuid {};
        auto raw_gpu_handle = uint {};
        const auto first_result = resource_manager.load_texture(texture_handle, first_resource);
        const auto second_result = resource_manager.load_texture(texture_handle, second_resource);
        const auto raw_result = resource_manager.load_texture(texture_handle, raw_gpu_handle);
        const auto usage = resource_manager.get_usage(texture_handle);

        // Assert
        ASSERT_TRUE(first_result);
        ASSERT_TRUE(second_result);
        ASSERT_TRUE(raw_result);
        ASSERT_TRUE(usage.has_value());
        EXPECT_EQ(first_resource, second_resource);
        EXPECT_EQ(static_cast<uint>(first_resource), raw_gpu_handle);
        EXPECT_EQ(backend.uploaded_texture_count, 1U);
        EXPECT_EQ(backend.recorded_texture_desc.size.width, 2U);
        EXPECT_EQ(backend.recorded_texture_desc.size.height, 2U);
        EXPECT_EQ(backend.recorded_texture_upload_size, 16U);
        EXPECT_EQ(usage->access_count, 3U);
    }

    // Validates GraphicsResourceManager uploads material pipelines and keeps dependencies hot.
    TEST(GraphicsResourceManagerTests, LoadMaterial_CachesUploadedPipeline)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto dispatcher = NullMessageDispatcher {};
        auto serialization_registry = SerializationRegistry {};
        serialization_registry.register_reader<Shader>(
            [](const std::filesystem::path&, const ShaderLoadParameters&)
            {
                return std::make_shared<Shader>(std::vector<ShaderSource> {
                    ShaderSource(
                        "#version 450 core\nvoid main(){ gl_Position = vec4(0.0); }\n",
                        ShaderType::VERTEX),
                    ShaderSource(
                        "#version 450 core\nlayout(location=0) out vec4 c; void main(){ c = "
                        "vec4(1.0); }\n",
                        ShaderType::FRAGMENT),
                });
            });
        serialization_registry.register_reader<Texture>(
            [](const std::filesystem::path&, const TextureLoadParameters&)
            {
                return std::make_shared<Texture>(
                    Size {1U, 1U},
                    TextureWrap::REPEAT,
                    TextureFilter::LINEAR,
                    TextureFormat::RGBA,
                    std::vector<Pixel> {255U, 255U, 255U, 255U});
            });
        serialization_registry.register_reader<Material>(
            [](const std::filesystem::path&, const MaterialLoadParameters&)
            {
                auto material = Material {};
                material.program.vertex = Handle("Shaders/Test.shader");
                material.program.fragment = Handle("Shaders/Test.shader");
                material.textures.set("diffuse_map", Handle("Textures/Diffuse.png"));
                return std::make_shared<Material>(std::move(material));
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto resource_manager = GraphicsResourceManager(backend, asset_manager, 3U);
        const auto material_handle = Handle("Materials/Test.mat");

        // Act
        auto first_resource = Uuid {};
        auto second_resource = Uuid {};
        const auto first_result = resource_manager.load_material(material_handle, first_resource);
        const auto second_result = resource_manager.load_material(material_handle, second_resource);
        const auto usage = resource_manager.get_usage(material_handle);
        const bool is_texture_loaded = resource_manager.is_loaded(Handle("Textures/Diffuse.png"));

        // Assert
        ASSERT_TRUE(first_result);
        ASSERT_TRUE(second_result);
        ASSERT_TRUE(usage.has_value());
        EXPECT_EQ(first_resource, second_resource);
        EXPECT_EQ(usage->access_count, 2U);
        EXPECT_EQ(backend.uploaded_pipeline_count, 1U);
        EXPECT_EQ(backend.uploaded_texture_count, 1U);
        EXPECT_TRUE(is_texture_loaded);
        EXPECT_EQ(backend.recorded_pipeline_desc.debug_name, "Material Materials/Test.mat");
    }

    // Validates GraphicsResourceManager uploads model mesh buffers as one cached resource group.
    TEST(GraphicsResourceManagerTests, LoadModel_CachesUploadedMeshBuffers)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto dispatcher = NullMessageDispatcher {};
        auto serialization_registry = SerializationRegistry {};
        serialization_registry.register_reader<Model>(
            [](const std::filesystem::path&, const ModelLoadParameters&)
            {
                return std::make_shared<Model>(triangle);
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto resource_manager = GraphicsResourceManager(backend, asset_manager, 2U);
        const auto model_handle = Handle("Models/Triangle.fbx");

        // Act
        auto first_resource = Uuid {};
        auto second_resource = Uuid {};
        const auto first_result = resource_manager.load_model(model_handle, first_resource);
        const auto second_result = resource_manager.load_model(model_handle, second_resource);
        const auto usage = resource_manager.get_usage(model_handle);
        const bool loaded_before_unload = resource_manager.is_loaded(model_handle);
        resource_manager.update();
        const uint unloaded_count = resource_manager.update();
        const bool loaded_after_unload = resource_manager.is_loaded(model_handle);

        // Assert
        ASSERT_TRUE(first_result);
        ASSERT_TRUE(second_result);
        ASSERT_TRUE(usage.has_value());
        EXPECT_EQ(first_resource, second_resource);
        EXPECT_EQ(usage->access_count, 2U);
        EXPECT_TRUE(loaded_before_unload);
        EXPECT_EQ(backend.uploaded_buffer_count, 2U);
        EXPECT_EQ(backend.recorded_buffer_descs[0U].usage, GraphicsBufferUsage::VERTEX);
        EXPECT_EQ(backend.recorded_buffer_descs[1U].usage, GraphicsBufferUsage::INDEX);
        EXPECT_EQ(unloaded_count, 1U);
        EXPECT_FALSE(loaded_after_unload);
        EXPECT_EQ(backend.unloaded_resources.size(), 2U);
    }

    // Validates cached model resources do not keep CPU model assets resident after upload.
    TEST(GraphicsResourceManagerTests, LoadModelResource_DoesNotRetainSourceAssetAfterUpload)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto dispatcher = NullMessageDispatcher {};
        auto serialization_registry = SerializationRegistry {};
        auto model_load_count = uint {};
        serialization_registry.register_reader<Model>(
            [&model_load_count](const std::filesystem::path&, const ModelLoadParameters&)
            {
                model_load_count += 1U;
                return std::make_shared<Model>(triangle);
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto resource_manager = GraphicsResourceManager(backend, asset_manager, 2U);
        const auto model_handle = Handle("Models/Triangle.fbx");

        // Act
        auto first_model_resource = GraphicsModelResource {};
        const auto first_result = resource_manager.load_model(model_handle, first_model_resource);
        asset_manager.unload_unreferenced();

        const AssetUsage usage_after_asset_cleanup = asset_manager.get_usage<Model>(model_handle);
        auto second_model_resource = GraphicsModelResource {};
        const auto second_result = resource_manager.load_model(model_handle, second_model_resource);
        const auto resource_usage = resource_manager.get_usage(model_handle);

        // Assert
        ASSERT_TRUE(first_result);
        ASSERT_TRUE(second_result);
        ASSERT_TRUE(resource_usage.has_value());
        EXPECT_EQ(model_load_count, 1U);
        EXPECT_EQ(usage_after_asset_cleanup.stream_state, AssetStreamState::UNLOADED);
        EXPECT_EQ(usage_after_asset_cleanup.ref_count, 0U);
        ASSERT_EQ(first_model_resource.meshes.size(), 1U);
        ASSERT_EQ(second_model_resource.meshes.size(), 1U);
        EXPECT_EQ(first_model_resource.resource, second_model_resource.resource);
        EXPECT_EQ(
            first_model_resource.meshes.front().vertex_buffer,
            second_model_resource.meshes.front().vertex_buffer);
        EXPECT_EQ(
            first_model_resource.meshes.front().index_buffer,
            second_model_resource.meshes.front().index_buffer);
        EXPECT_EQ(resource_usage->access_count, 2U);
        EXPECT_EQ(backend.uploaded_buffer_count, 2U);
    }

    // Validates GraphicsResourceManager evicts textures that are not used for the frame window.
    TEST(GraphicsResourceManagerTests, Update_UnloadsStaleTextureResources)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto dispatcher = NullMessageDispatcher {};
        auto serialization_registry = SerializationRegistry {};
        serialization_registry.register_reader<Texture>(
            [](const std::filesystem::path&, const TextureLoadParameters&)
            {
                return std::make_shared<Texture>(
                    Size {1U, 1U},
                    TextureWrap::REPEAT,
                    TextureFilter::LINEAR,
                    TextureFormat::RGBA,
                    std::vector<Pixel> {255U, 255U, 255U, 255U});
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto resource_manager = GraphicsResourceManager(backend, asset_manager, 3U);
        const auto texture_handle = Handle("Textures/Stale.png");

        auto first_resource = Uuid {};
        const auto load_result = resource_manager.load_texture(texture_handle, first_resource);

        // Act
        const uint first_unload_count = resource_manager.update();
        const uint second_unload_count = resource_manager.update();
        const bool loaded_before_limit = resource_manager.is_loaded(texture_handle);
        const uint third_unload_count = resource_manager.update();
        const bool loaded_after_limit = resource_manager.is_loaded(texture_handle);

        auto second_resource = Uuid {};
        const auto reload_result = resource_manager.load_texture(texture_handle, second_resource);

        // Assert
        ASSERT_TRUE(load_result);
        ASSERT_TRUE(reload_result);
        EXPECT_EQ(first_unload_count, 0U);
        EXPECT_EQ(second_unload_count, 0U);
        EXPECT_TRUE(loaded_before_limit);
        EXPECT_EQ(third_unload_count, 1U);
        EXPECT_FALSE(loaded_after_limit);
        ASSERT_EQ(backend.unloaded_resources.size(), 1U);
        EXPECT_EQ(backend.unloaded_resources.front(), first_resource);
        EXPECT_NE(first_resource, second_resource);
        EXPECT_EQ(backend.uploaded_texture_count, 2U);
    }
}
