#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/pipeline/context/frame_data.h"
#include "tbx/systems/graphics/pipeline/render_pipeline.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/model.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/material.h"
#include "tbx/types/shader.h"
#include "tbx/types/texture.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <thread>
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

    class RecordingGraphicsBackend : public IGraphicsBackend
    {
      public:
        Result begin_frame(const GraphicsFrameInfo& frame) override
        {
            recorded_output_window = frame.output_window;
            recorded_render_resolution = frame.render_resolution;
            begin_frame_thread_id = std::this_thread::get_id();
            callbacks.push_back(GraphicsBackendCallback::BeginFrame);
            return {};
        }

        Result begin_pass(const GraphicsPassDesc& pass) override
        {
            recorded_pass = pass;
            recorded_passes.push_back(pass);
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
            recorded_pipelines.push_back(pipeline_resource_uuid);
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
            initialize_thread_id = std::this_thread::get_id();
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
            wait_for_idle_thread_id = std::this_thread::get_id();
            callbacks.push_back(GraphicsBackendCallback::WaitForIdle);
        }

      public:
        std::vector<GraphicsBackendCallback> callbacks = {};
        std::thread::id begin_frame_thread_id = {};
        std::thread::id initialize_thread_id = {};
        Window recorded_output_window = {};
        Size recorded_render_resolution = {};
        Size recorded_viewport = {};
        Size recorded_scissor = {};
        GraphicsPassDesc recorded_pass = {};
        std::vector<GraphicsPassDesc> recorded_passes = {};
        Uuid recorded_pipeline = {};
        std::vector<Uuid> recorded_pipelines = {};
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
        std::thread::id wait_for_idle_thread_id = {};
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
        Window open(const WindowCreateInfo& create_info = {}) override
        {
            (void)create_info;
            is_window_open = true;
            return window;
        }

        bool close(const Window& target_window) override
        {
            if (target_window != window)
                return false;

            is_window_open = false;
            return true;
        }

        bool has(const Window& target_window) const override
        {
            return target_window == window && is_window_open;
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

        void update() override {}

        void shutdown() override
        {
            is_window_open = false;
        }

      public:
        Window window = Window("main");
        Size size = Size {1280U, 720U};
        bool is_window_open = true;
    };

    class BlockingGraphicsBackend final : public RecordingGraphicsBackend
    {
      public:
        BlockingGraphicsBackend(std::shared_future<void> allow_begin_frame)
            : _allow_begin_frame(std::move(allow_begin_frame))
        {
        }

        Result begin_frame(const GraphicsFrameInfo& frame) override
        {
            auto result = RecordingGraphicsBackend::begin_frame(frame);
            _begin_frame_started.set_value();
            _allow_begin_frame.wait();
            return result;
        }

        std::future<void> take_begin_frame_started_future()
        {
            return _begin_frame_started.get_future();
        }

      private:
        std::shared_future<void> _allow_begin_frame = {};
        std::promise<void> _begin_frame_started = {};
    };

    class InitBlockingGraphicsBackend final : public RecordingGraphicsBackend
    {
      public:
        InitBlockingGraphicsBackend(std::shared_future<void> allow_initialize)
            : _allow_initialize(std::move(allow_initialize))
        {
        }

        Result initialize(const GraphicsSettings& settings) override
        {
            auto result = RecordingGraphicsBackend::initialize(settings);
            _initialize_started.set_value();
            _allow_initialize.wait();
            return result;
        }

        std::future<void> take_initialize_started_future()
        {
            return _initialize_started.get_future();
        }

      private:
        std::shared_future<void> _allow_initialize = {};
        std::promise<void> _initialize_started = {};
    };

    static void wait_for_render_lane(ThreadManager& thread_manager)
    {
        auto completion = thread_manager.post_with_future(
            "render",
            []()
            {
            });
        completion.get();
    }

    template <typename TService>
    static std::shared_ptr<TService> make_non_owning_service(TService& service)
    {
        return std::shared_ptr<TService>(
            &service,
            [](TService*)
            {
            });
    }

    // Validates Rendering opens frame state and submits geometry through render().
    TEST(RenderingTests, Render_DelegatesFrameAndGeometryPassCommands)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto registry = EntityRegistry {};
        auto thread_manager = ThreadManager {};
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
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto registry_service = make_non_owning_service(registry);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            registry_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service,
            window_manager.window,
            settings);
        rendering.render();
        wait_for_render_lane(thread_manager);

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

        EXPECT_EQ(backend.recorded_output_window.get_id(), window_manager.window.get_id());
        EXPECT_EQ(backend.recorded_render_resolution.width, window_manager.size.width);
        EXPECT_EQ(backend.recorded_render_resolution.height, window_manager.size.height);
        EXPECT_EQ(backend.recorded_viewport.width, window_manager.size.width);
        EXPECT_EQ(backend.recorded_viewport.height, window_manager.size.height);
        ASSERT_FALSE(backend.recorded_passes.empty());
        EXPECT_EQ(backend.recorded_passes[0U].clear_flags, GraphicsClearFlags::COLOR_DEPTH);
        EXPECT_EQ(backend.recorded_passes[0U].debug_name, "Toybox Opaque Scene Pass");
        for (const auto expected_callback : expected_callbacks)
        {
            EXPECT_NE(
                std::find(backend.callbacks.begin(), backend.callbacks.end(), expected_callback),
                backend.callbacks.end());
        }
    }

    // Validates initialization runs asynchronously and the first render waits for it.
    TEST(RenderingTests, ConstructorDoesNotWaitForInitializationAndFirstRenderDoes)
    {
        // Arrange
        auto allow_initialize = std::promise<void> {};
        auto backend = InitBlockingGraphicsBackend(allow_initialize.get_future().share());
        auto initialize_started = backend.take_initialize_started_future();
        auto registry = EntityRegistry {};
        auto thread_manager = ThreadManager {};
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
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto registry_service = make_non_owning_service(registry);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            registry_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service,
            window_manager.window,
            settings);
        ASSERT_EQ(initialize_started.wait_for(std::chrono::seconds(1)), std::future_status::ready);
        auto lane_drain = thread_manager.post_with_future(
            "render",
            []()
            {
            });
        auto render_call = std::async(
            std::launch::async,
            [&rendering]()
            {
                rendering.render();
            });

        // Assert
        EXPECT_EQ(lane_drain.wait_for(std::chrono::milliseconds(10)), std::future_status::timeout);
        EXPECT_EQ(render_call.wait_for(std::chrono::milliseconds(10)), std::future_status::timeout);

        // Cleanup
        allow_initialize.set_value();
        lane_drain.get();
        render_call.get();
        wait_for_render_lane(thread_manager);
    }

    // Validates render() returns before the submitted frame finishes on the render lane.
    TEST(RenderingTests, Render_ReturnsBeforeSubmittedFrameCompletes)
    {
        // Arrange
        auto allow_begin_frame = std::promise<void> {};
        auto backend = BlockingGraphicsBackend(allow_begin_frame.get_future().share());
        auto begin_frame_started = backend.take_begin_frame_started_future();
        auto registry = EntityRegistry {};
        auto thread_manager = ThreadManager {};
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
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto registry_service = make_non_owning_service(registry);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto rendering = Rendering(
            backend_service,
            registry_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service,
            window_manager.window,
            settings);

        // Act
        rendering.render();
        ASSERT_EQ(begin_frame_started.wait_for(std::chrono::seconds(1)), std::future_status::ready);
        auto lane_drain = thread_manager.post_with_future(
            "render",
            []()
            {
            });

        // Assert
        EXPECT_EQ(lane_drain.wait_for(std::chrono::milliseconds(10)), std::future_status::timeout);

        // Cleanup
        allow_begin_frame.set_value();
        lane_drain.get();
    }

    // Validates renderer lifecycle work stays on the dedicated render lane.
    TEST(RenderingTests, Render_UsesDedicatedRenderLaneForLifecycleWork)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto registry = EntityRegistry {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = NullMessageDispatcher {};
        auto serialization_registry = SerializationRegistry {};
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto settings =
            GraphicsSettings(dispatcher, false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        const auto caller_thread_id = std::this_thread::get_id();
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto registry_service = make_non_owning_service(registry);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        {
            auto rendering = Rendering(
                backend_service,
                registry_service,
                asset_manager_service,
                thread_manager_service,
                window_manager_service,
                window_manager.window,
                settings);
            rendering.render();
            wait_for_render_lane(thread_manager);
        }

        // Assert
        EXPECT_NE(backend.initialize_thread_id, std::thread::id {});
        EXPECT_NE(backend.begin_frame_thread_id, std::thread::id {});
        EXPECT_NE(backend.wait_for_idle_thread_id, std::thread::id {});
        EXPECT_NE(backend.initialize_thread_id, caller_thread_id);
        EXPECT_EQ(backend.initialize_thread_id, backend.begin_frame_thread_id);
        EXPECT_EQ(backend.begin_frame_thread_id, backend.wait_for_idle_thread_id);
        EXPECT_FALSE(thread_manager.has_lane("render"));
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

    struct RenderPipelineOperationState
    {
        bool prepared = false;
        bool executed = false;
    };

    class RecordingRenderOperation final : public IRenderOperation
    {
      public:
        RecordingRenderOperation(RenderPipelineOperationState& state)
            : _state(state)
        {
        }

      public:
        RenderOperationDebugInfo get_debug_info() const override
        {
            auto debug_info = RenderOperationDebugInfo();
            debug_info.debug_name = "Recording Operation";
            debug_info.category = "Tests";
            return debug_info;
        }

        Result prepare(RenderData& render_data) override
        {
            _state.get().prepared = render_data.frame_index == 42U;
            return {};
        }

        Result execute(IGraphicsBackend& backend, RenderData&, const CancellationToken&) override
        {
            _state.get().executed = true;
            return backend.set_viewport(
                Viewport {
                    .position = Vec2(0.0F),
                    .dimensions = Size {64U, 64U},
                });
        }

      private:
        std::reference_wrapper<RenderPipelineOperationState> _state;
    };

    // Validates typed render pipelines prepare and execute owned operations.
    TEST(RenderPipelineTests, PrepareExecute_RunsOwnedOperations)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto dispatcher = NullMessageDispatcher {};
        auto serialization_registry = SerializationRegistry {};
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto resource_manager = GraphicsResourceManager(
            make_non_owning_service<IGraphicsBackend>(backend),
            make_non_owning_service(asset_manager),
            3U);
        auto registry = EntityRegistry {};
        auto window_manager = RecordingWindowManager {};
        auto render_data = std::make_unique<RenderData>();
        render_data->output_window = window_manager.window;
        render_data->frame_index = 42U;
        auto state = RenderPipelineOperationState {};
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto pipeline = RenderPipeline(backend_service);
        pipeline.add_operation(std::make_unique<RecordingRenderOperation>(state));

        // Act
        const auto prepare_result = pipeline.prepare(std::move(render_data));
        const auto execute_result = pipeline.execute(CancellationToken {});
        pipeline.clear();

        // Assert
        const auto expected_callbacks = std::vector<GraphicsBackendCallback> {
            GraphicsBackendCallback::SetViewport,
        };

        EXPECT_TRUE(prepare_result);
        EXPECT_TRUE(execute_result);
        EXPECT_TRUE(state.prepared);
        EXPECT_TRUE(state.executed);
        EXPECT_EQ(backend.callbacks, expected_callbacks);
    }

    // Validates Rendering owns the Toybox geometry pass and submits render() commands.
    TEST(RenderPipelineTests, Rendering_RenderSubmitsGeometryPass)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto registry = EntityRegistry {};
        auto thread_manager = ThreadManager {};
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
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto registry_service = make_non_owning_service(registry);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            registry_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service,
            window_manager.window,
            settings);
        rendering.render();
        wait_for_render_lane(thread_manager);

        // Assert
        ASSERT_EQ(backend.recorded_passes.size(), 2U);
        EXPECT_EQ(backend.recorded_passes[0U].debug_name, "Toybox Opaque Scene Pass");
        EXPECT_EQ(backend.recorded_passes[0U].clear_flags, GraphicsClearFlags::COLOR_DEPTH);
        EXPECT_EQ(backend.recorded_passes[1U].debug_name, "Toybox Lighting Pass");
        EXPECT_EQ(backend.recorded_passes[1U].clear_flags, GraphicsClearFlags::COLOR_DEPTH);
        EXPECT_NE(
            std::find(
                backend.callbacks.begin(),
                backend.callbacks.end(),
                GraphicsBackendCallback::DrawIndexed),
            backend.callbacks.end());
        EXPECT_NE(
            std::find(
                backend.callbacks.begin(),
                backend.callbacks.end(),
                GraphicsBackendCallback::Draw),
            backend.callbacks.end());
    }

    // Validates Sky entities submit a dedicated skybox pass before the geometry pass.
    TEST(RenderingTests, Render_SkyComponentSubmitsSkyboxPassBeforeGeometryPass)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto registry = EntityRegistry {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
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
        serialization_registry.register_reader<Material>(
            [](const std::filesystem::path&, const MaterialLoadParameters&)
            {
                auto material = Material {};
                material.program.vertex = Handle("Shaders/Sky.shader");
                material.program.fragment = Handle("Shaders/Sky.shader");
                material.textures.set("diffuse_map", Handle {});
                material.parameters.set("color", Color(0.25F, 0.5F, 1.0F, 1.0F));
                return std::make_shared<Material>(std::move(material));
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto settings =
            GraphicsSettings(dispatcher, false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto sky_entity = Entity("Sky", registry);
        sky_entity.add_component<Sky>(Sky {
            .material = MaterialInstance(Handle("Materials/Sky.mat")),
        });
        auto mesh_entity = Entity("Triangle", registry);
        mesh_entity.add_component<DynamicMesh>(triangle);
        mesh_entity.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto registry_service = make_non_owning_service(registry);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            registry_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service,
            window_manager.window,
            settings);
        rendering.render();
        wait_for_render_lane(thread_manager);

        // Assert
        ASSERT_EQ(backend.recorded_passes.size(), 3U);
        EXPECT_EQ(backend.recorded_passes[0U].debug_name, "Toybox Skybox Pass");
        EXPECT_EQ(backend.recorded_passes[0U].clear_flags, GraphicsClearFlags::COLOR_DEPTH);
        EXPECT_EQ(backend.recorded_passes[1U].debug_name, "Toybox Opaque Scene Pass");
        EXPECT_EQ(backend.recorded_passes[1U].clear_flags, GraphicsClearFlags::DEPTH);
        EXPECT_EQ(backend.recorded_passes[2U].debug_name, "Toybox Lighting Pass");
        EXPECT_EQ(backend.recorded_passes[2U].clear_flags, GraphicsClearFlags::COLOR_DEPTH);
        EXPECT_NE(
            std::find(
                backend.callbacks.begin(),
                backend.callbacks.end(),
                GraphicsBackendCallback::DrawIndexed),
            backend.callbacks.end());
        EXPECT_NE(
            std::find(
                backend.callbacks.begin(),
                backend.callbacks.end(),
                GraphicsBackendCallback::Draw),
            backend.callbacks.end());
    }

    // Validates static mesh rendering uses cached model buffers without reloading CPU assets.
    TEST(RenderingTests, Render_StaticMeshUsesCachedModelResourceWithoutReloadingAsset)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto registry = EntityRegistry {};
        auto thread_manager = ThreadManager {};
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
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto registry_service = make_non_owning_service(registry);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto rendering = Rendering(
            backend_service,
            registry_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service,
            window_manager.window,
            settings);

        // Act
        rendering.render();
        wait_for_render_lane(thread_manager);
        asset_manager.unload_unreferenced();
        const AssetUsage usage_after_asset_cleanup = asset_manager.get_usage<Model>(model_handle);
        rendering.render();
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_EQ(model_load_count, 1U);
        EXPECT_EQ(usage_after_asset_cleanup.stream_state, AssetStreamState::UNLOADED);
        EXPECT_GE(backend.uploaded_buffer_count, 3U);
        EXPECT_GE(backend.uploaded_pipeline_count, 2U);
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
        auto resource_manager = GraphicsResourceManager(
            make_non_owning_service<IGraphicsBackend>(backend),
            make_non_owning_service(asset_manager),
            3U);
        const auto texture_handle = Handle("Textures/Diffuse.png");

        // Act
        auto first_resource = Uuid {};
        auto second_resource = Uuid {};
        auto raw_gpu_handle = uint {};
        const auto first_result =
            resource_manager.upload(texture_handle, TextureLoadParameters {}, first_resource);
        const auto second_result =
            resource_manager.upload(texture_handle, TextureLoadParameters {}, second_resource);
        const auto raw_result =
            resource_manager.upload(texture_handle, TextureLoadParameters {}, raw_gpu_handle);
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
        auto resource_manager = GraphicsResourceManager(
            make_non_owning_service<IGraphicsBackend>(backend),
            make_non_owning_service(asset_manager),
            3U);
        const auto material_handle = Handle("Materials/Test.mat");

        // Act
        auto first_resource = Uuid {};
        auto second_resource = Uuid {};
        const auto first_result =
            resource_manager.upload(material_handle, MaterialLoadParameters {}, first_resource);
        const auto second_result =
            resource_manager.upload(material_handle, MaterialLoadParameters {}, second_resource);
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

    // Validates active material/texture resources keep source assets pinned between cleanup ticks.
    TEST(GraphicsResourceManagerTests, LoadMaterial_KeepsAssetsPinnedWhileResourceTracked)
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
        auto resource_manager = GraphicsResourceManager(
            make_non_owning_service<IGraphicsBackend>(backend),
            make_non_owning_service(asset_manager),
            2U);

        const auto material_handle = Handle("Materials/Test.mat");
        const auto texture_handle = Handle("Textures/Diffuse.png");
        auto material_resource = Uuid {};

        // Act
        const auto load_result =
            resource_manager.upload(material_handle, MaterialLoadParameters {}, material_resource);
        asset_manager.unload_unreferenced();
        const AssetUsage material_usage_while_tracked =
            asset_manager.get_usage<Material>(material_handle);
        const AssetUsage texture_usage_while_tracked =
            asset_manager.get_usage<Texture>(texture_handle);

        resource_manager.unload_stale();
        resource_manager.unload_stale();
        asset_manager.unload_unreferenced();
        const AssetUsage material_usage_after_eviction =
            asset_manager.get_usage<Material>(material_handle);
        const AssetUsage texture_usage_after_eviction =
            asset_manager.get_usage<Texture>(texture_handle);

        // Assert
        ASSERT_TRUE(load_result);
        EXPECT_EQ(material_usage_while_tracked.stream_state, AssetStreamState::LOADED);
        EXPECT_EQ(texture_usage_while_tracked.stream_state, AssetStreamState::LOADED);
        EXPECT_TRUE(material_usage_while_tracked.is_pinned);
        EXPECT_TRUE(texture_usage_while_tracked.is_pinned);

        EXPECT_EQ(material_usage_after_eviction.stream_state, AssetStreamState::UNLOADED);
        EXPECT_EQ(texture_usage_after_eviction.stream_state, AssetStreamState::UNLOADED);
        EXPECT_FALSE(material_usage_after_eviction.is_pinned);
        EXPECT_FALSE(texture_usage_after_eviction.is_pinned);
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
        auto resource_manager = GraphicsResourceManager(
            make_non_owning_service<IGraphicsBackend>(backend),
            make_non_owning_service(asset_manager),
            2U);
        const auto model_handle = Handle("Models/Triangle.fbx");

        // Act
        auto first_resource = Uuid {};
        auto second_resource = Uuid {};
        const auto first_result =
            resource_manager.upload(model_handle, ModelLoadParameters {}, first_resource);
        const auto second_result =
            resource_manager.upload(model_handle, ModelLoadParameters {}, second_resource);
        const auto usage = resource_manager.get_usage(model_handle);
        const bool loaded_before_unload = resource_manager.is_loaded(model_handle);
        resource_manager.unload_stale();
        const uint unloaded_count = resource_manager.unload_stale();
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
        auto resource_manager = GraphicsResourceManager(
            make_non_owning_service<IGraphicsBackend>(backend),
            make_non_owning_service(asset_manager),
            2U);
        const auto model_handle = Handle("Models/Triangle.fbx");

        // Act
        auto first_model_resource = GraphicsModelResource {};
        const auto first_result =
            resource_manager.upload(model_handle, ModelLoadParameters {}, first_model_resource);
        asset_manager.unload_unreferenced();

        const AssetUsage usage_after_asset_cleanup = asset_manager.get_usage<Model>(model_handle);
        auto second_model_resource = GraphicsModelResource {};
        const auto second_result =
            resource_manager.upload(model_handle, ModelLoadParameters {}, second_model_resource);
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
        auto resource_manager = GraphicsResourceManager(
            make_non_owning_service<IGraphicsBackend>(backend),
            make_non_owning_service(asset_manager),
            3U);
        const auto texture_handle = Handle("Textures/Stale.png");

        auto first_resource = Uuid {};
        const auto load_result =
            resource_manager.upload(texture_handle, TextureLoadParameters {}, first_resource);

        // Act
        const uint first_unload_count = resource_manager.unload_stale();
        const uint second_unload_count = resource_manager.unload_stale();
        const bool loaded_before_limit = resource_manager.is_loaded(texture_handle);
        const uint third_unload_count = resource_manager.unload_stale();
        const bool loaded_after_limit = resource_manager.is_loaded(texture_handle);

        auto second_resource = Uuid {};
        const auto reload_result =
            resource_manager.upload(texture_handle, TextureLoadParameters {}, second_resource);

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
