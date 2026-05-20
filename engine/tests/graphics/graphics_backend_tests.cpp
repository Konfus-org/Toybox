#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/model.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/material.h"
#include "tbx/types/shader.h"
#include "tbx/types/texture.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
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

    struct RecordedBufferUpload
    {
        GraphicsBufferDesc desc = {};
        std::vector<uint8> data = {};
    };

    class RecordingGraphicsBackend : public IGraphicsBackend
    {
      public:
        Result begin_frame(const Window& output_target) override
        {
            recorded_output_window = output_target;
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

        Result begin_view(const RenderView& view) override
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
            recorded_vertex_slots.push_back(slot);
            recorded_vertex_buffers.push_back(buffer_resource_uuid);
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
            recorded_draws.push_back(draw);
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

        Result update_buffer(const Uuid& resource_uuid, const void*, uint64, uint64) override
        {
            updated_buffer_count += 1U;
            updated_buffers.push_back(resource_uuid);
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
            const void* data,
            uint64 data_size,
            Uuid& out_resource_uuid) override
        {
            recorded_buffer_descs.push_back(desc);
            auto upload = RecordedBufferUpload {.desc = desc};
            if (data != nullptr && data_size > 0U)
            {
                const auto* bytes = static_cast<const uint8*>(data);
                upload.data.assign(bytes, bytes + static_cast<size>(data_size));
            }
            recorded_buffer_uploads.push_back(std::move(upload));
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
            const void* data,
            uint64 data_size,
            Uuid& out_resource_uuid) override
        {
            recorded_texture_desc = desc;
            recorded_texture_upload_data.clear();
            if (data != nullptr && data_size > 0U)
            {
                const auto* bytes = static_cast<const uint8*>(data);
                recorded_texture_upload_data.assign(bytes, bytes + static_cast<size>(data_size));
            }
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
        Size recorded_viewport = {};
        Size recorded_scissor = {};
        GraphicsPassDesc recorded_pass = {};
        std::vector<GraphicsPassDesc> recorded_passes = {};
        Uuid recorded_pipeline = {};
        std::vector<Uuid> recorded_pipelines = {};
        Uuid recorded_vertex_buffer = {};
        std::vector<Uuid> recorded_vertex_buffers = {};
        Uuid recorded_index_buffer = {};
        Uuid recorded_uniform_buffer = {};
        Uuid recorded_texture = {};
        Uuid recorded_sampler = {};
        uint32 recorded_vertex_slot = 0U;
        std::vector<uint32> recorded_vertex_slots = {};
        uint32 recorded_uniform_slot = 0U;
        uint32 recorded_texture_slot = 0U;
        uint32 recorded_sampler_slot = 0U;
        GraphicsIndexType recorded_index_type = GraphicsIndexType::UINT32;
        GraphicsDrawIndexedDesc recorded_draw = {};
        std::vector<GraphicsDrawIndexedDesc> recorded_draws = {};
        GraphicsPipelineDesc recorded_pipeline_desc = {};
        std::vector<GraphicsBufferDesc> recorded_buffer_descs = {};
        std::vector<RecordedBufferUpload> recorded_buffer_uploads = {};
        GraphicsTextureDesc recorded_texture_desc = {};
        std::vector<uint8> recorded_texture_upload_data = {};
        std::vector<Uuid> unloaded_resources = {};
        uint64 recorded_texture_upload_size = 0U;
        uint uploaded_buffer_count = 0U;
        uint uploaded_pipeline_count = 0U;
        uint uploaded_texture_count = 0U;
        uint updated_buffer_count = 0U;
        uint32 next_uploaded_resource = 1000U;
        std::vector<Uuid> updated_buffers = {};
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
            if (!main_window.is_valid())
                main_window = window;
            return window;
        }

        bool close(const Window& target_window) override
        {
            if (target_window != window)
                return false;

            is_window_open = false;
            if (main_window == target_window)
                main_window = {};
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

        bool has_main_window() const override
        {
            return main_window.is_valid() && is_window_open;
        }

        const Window& get_main_window() const override
        {
            return main_window;
        }

        bool set_main_window(const Window& next_main_window) override
        {
            if (!is_window_open || next_main_window != window)
                return false;

            main_window = next_main_window;
            return true;
        }

        void update() override {}

        void shutdown() override
        {
            is_window_open = false;
        }

      public:
        Window window = Window("main");
        Window main_window = window;
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

        Result begin_frame(const Window& output_target) override
        {
            auto result = RecordingGraphicsBackend::begin_frame(output_target);
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

    static std::shared_ptr<Material> make_test_material_with_texture(const Handle& texture_handle)
    {
        auto material = Material {};
        material.textures.set("u_albedo_map", texture_handle);
        return std::make_shared<Material>(std::move(material));
    }

    static std::optional<CameraShaderData> find_camera_shader_data(
        const std::vector<RecordedBufferUpload>& uploads)
    {
        for (const auto& upload : uploads)
        {
            if (upload.desc.debug_name != "Camera Shader Data"
                || upload.data.size() < sizeof(CameraShaderData))
                continue;

            auto shader_data = CameraShaderData {};
            std::memcpy(&shader_data, upload.data.data(), sizeof(CameraShaderData));
            return shader_data;
        }

        return std::nullopt;
    }

    static std::optional<ObjectShaderData> find_object_shader_data(
        const std::vector<RecordedBufferUpload>& uploads)
    {
        for (const auto& upload : uploads)
        {
            if (upload.desc.debug_name != "Object Shader Data"
                || upload.data.size() < sizeof(ObjectShaderData))
                continue;

            auto shader_data = ObjectShaderData {};
            std::memcpy(&shader_data, upload.data.data(), sizeof(ObjectShaderData));
            return shader_data;
        }

        return std::nullopt;
    }

    static std::optional<LightShaderData> find_light_shader_data(
        const std::vector<RecordedBufferUpload>& uploads)
    {
        for (const auto& upload : uploads)
        {
            if (upload.desc.debug_name != "Light Shader Data"
                || upload.data.size() < sizeof(LightShaderData))
                continue;

            auto shader_data = LightShaderData {};
            std::memcpy(&shader_data, upload.data.data(), sizeof(LightShaderData));
            return shader_data;
        }

        return std::nullopt;
    }

    static uint count_dynamic_mesh_vertex_uploads(const std::vector<RecordedBufferUpload>& uploads)
    {
        auto count = uint {};
        for (const auto& upload : uploads)
        {
            if (upload.desc.debug_name.find("DynamicMesh") != std::string::npos
                && upload.desc.debug_name.find("Vertices") != std::string::npos)
            {
                count += 1U;
            }
        }

        return count;
    }

    static bool contains_vertex_slot(const RecordingGraphicsBackend& backend, const uint32 slot)
    {
        return std::find(
                   backend.recorded_vertex_slots.begin(),
                   backend.recorded_vertex_slots.end(),
                   slot)
               != backend.recorded_vertex_slots.end();
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

    // Validates Rendering owns the Toybox geometry pass and submits render() commands.
    TEST(RenderingTests, Rendering_RenderSubmitsGeometryPass)
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
            settings);
        rendering.render();
        wait_for_render_lane(thread_manager);

        // Assert
        ASSERT_EQ(backend.recorded_passes.size(), 2U);
        EXPECT_EQ(backend.recorded_passes[0U].debug_name, "Toybox Opaque Scene Pass");
        EXPECT_EQ(backend.recorded_passes[0U].clear_flags, GraphicsClearFlags::COLOR_DEPTH);
        EXPECT_EQ(backend.recorded_passes[1U].debug_name, "Toybox Lighting Pass");
        EXPECT_EQ(backend.recorded_passes[1U].clear_flags, GraphicsClearFlags::NONE);
        EXPECT_NE(
            std::find(
                backend.callbacks.begin(),
                backend.callbacks.end(),
                GraphicsBackendCallback::DrawIndexed),
            backend.callbacks.end());
    }

    // Validates render uniforms are built from ECS world transforms, not local transforms.
    TEST(RenderingTests, Render_UploadsWorldSpaceTransformUniforms)
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
        auto root = Entity("Root", registry);
        root.add_component<Transform>(Vec3(10.0F, 0.0F, 0.0F));
        auto camera = Entity("Camera", root.get_id(), registry);
        camera.add_component<Camera>();
        camera.add_component<Transform>(Vec3(0.0F, 2.0F, 11.0F));
        auto mesh = Entity("Mesh", root.get_id(), registry);
        mesh.add_component<DynamicMesh>(triangle);
        mesh.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
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
            settings);
        rendering.render();
        wait_for_render_lane(thread_manager);
        const auto camera_shader_data = find_camera_shader_data(backend.recorded_buffer_uploads);
        const auto object_shader_data = find_object_shader_data(backend.recorded_buffer_uploads);

        // Assert
        ASSERT_TRUE(camera_shader_data.has_value());
        EXPECT_FLOAT_EQ(camera_shader_data->world_position.x, 10.0F);
        EXPECT_FLOAT_EQ(camera_shader_data->world_position.y, 2.0F);
        EXPECT_FLOAT_EQ(camera_shader_data->world_position.z, 11.0F);
        ASSERT_TRUE(object_shader_data.has_value());
        EXPECT_FLOAT_EQ(object_shader_data->model[3].x, 10.0F);
        EXPECT_FLOAT_EQ(object_shader_data->model[3].y, 0.0F);
        EXPECT_FLOAT_EQ(object_shader_data->model[3].z, -2.0F);
    }

    // Validates light UBO uploads match the std140 shader block size and contain visible lighting.
    TEST(RenderingTests, Render_UploadsFullLightUniformBlock)
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
        auto camera = Entity("Camera", registry);
        camera.add_component<Camera>();
        camera.add_component<Transform>(Vec3(0.0F, 0.0F, 5.0F));
        auto sun = Entity("Sun", registry);
        sun.add_component<DirectionalLight>(Color(1.0F, 0.5F, 0.25F, 1.0F), 2.0F, 0.15F);
        sun.add_component<Transform>();
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
            settings);
        rendering.render();
        wait_for_render_lane(thread_manager);
        const auto light_shader_data = find_light_shader_data(backend.recorded_buffer_uploads);

        // Assert
        EXPECT_EQ(sizeof(ShaderLightData), 64U);
        EXPECT_EQ(offsetof(LightShaderData, lights), 48U);
        ASSERT_TRUE(light_shader_data.has_value());
        EXPECT_EQ(light_shader_data->light_meta.x, 1);
        EXPECT_FLOAT_EQ(light_shader_data->ambient_color.x, 0.15F);
        EXPECT_FLOAT_EQ(light_shader_data->ambient_color.y, 0.075F);
        EXPECT_FLOAT_EQ(light_shader_data->ambient_color.z, 0.0375F);
        EXPECT_FLOAT_EQ(light_shader_data->lights[0U].position_type.w, 0.0F);
        EXPECT_FLOAT_EQ(light_shader_data->lights[0U].color_intensity.w, 2.0F);
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
        serialization_registry.register_reader<ShaderProgram>(
            [](const std::filesystem::path&, const ShaderLoadParameters&)
            {
                return std::make_shared<ShaderProgram>(std::vector<ShaderSource> {
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
                material.shader.vertex = Handle("Shaders/Sky.shader");
                material.shader.fragment = Handle("Shaders/Sky.shader");
                material.textures.set("u_albedo_map", Handle {});
                material.parameters.set("u_albedo_color", Color(0.25F, 0.5F, 1.0F, 1.0F));
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
        sky_entity.add_component<Transform>();
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
        EXPECT_EQ(backend.recorded_passes[2U].clear_flags, GraphicsClearFlags::NONE);
        EXPECT_NE(
            std::find(
                backend.callbacks.begin(),
                backend.callbacks.end(),
                GraphicsBackendCallback::DrawIndexed),
            backend.callbacks.end());
    }

    // Validates renderer caches GPU material resources without retaining CPU material assets.
    TEST(RenderingTests, Render_MaterialAssetCanUnloadAfterUpload)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto registry = EntityRegistry {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = NullMessageDispatcher {};
        auto serialization_registry = SerializationRegistry {};
        auto material_load_count = uint {};
        serialization_registry.register_reader<Material>(
            [&material_load_count](const std::filesystem::path&, const MaterialLoadParameters&)
            {
                material_load_count += 1U;
                auto material = Material {};
                material.parameters.set("u_albedo_color", Color(0.2F, 0.4F, 0.6F, 1.0F));
                return std::make_shared<Material>(std::move(material));
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto settings =
            GraphicsSettings(dispatcher, false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        const auto material_handle = Handle("Materials/Transient.mat");
        auto entity = Entity("MaterialTriangle", registry);
        entity.add_component<DynamicMesh>(triangle);
        entity.add_component<MaterialInstance>(MaterialInstance(material_handle));
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
            settings);

        // Act
        rendering.render();
        wait_for_render_lane(thread_manager);
        asset_manager.unload_unreferenced();
        const AssetUsage material_usage_after_asset_cleanup =
            asset_manager.get_usage<Material>(material_handle);

        // Assert
        EXPECT_EQ(material_load_count, 1U);
        EXPECT_EQ(material_usage_after_asset_cleanup.stream_state, AssetStreamState::UNLOADED);
        EXPECT_EQ(material_usage_after_asset_cleanup.ref_count, 0U);
        EXPECT_GE(backend.uploaded_pipeline_count, 1U);
    }

    // Validates static mesh rendering reuses uploaded model buffers after CPU asset cleanup.
    TEST(RenderingTests, Render_StaticMeshReusesUploadedModelBuffers)
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
            settings);

        // Act
        rendering.render();
        wait_for_render_lane(thread_manager);
        asset_manager.unload_unreferenced();
        const AssetUsage usage_after_asset_cleanup = asset_manager.get_usage<Model>(model_handle);
        rendering.render();
        wait_for_render_lane(thread_manager);
        rendering.render();
        wait_for_render_lane(thread_manager);
        const uint uploaded_buffer_count_after_ring_warmup = backend.uploaded_buffer_count;
        const uint updated_buffer_count_after_ring_warmup = backend.updated_buffer_count;
        rendering.render();
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_EQ(model_load_count, 1U);
        EXPECT_EQ(usage_after_asset_cleanup.stream_state, AssetStreamState::UNLOADED);
        EXPECT_EQ(backend.uploaded_buffer_count, uploaded_buffer_count_after_ring_warmup);
        EXPECT_GT(backend.updated_buffer_count, updated_buffer_count_after_ring_warmup);
        EXPECT_GE(backend.uploaded_pipeline_count, 1U);
        EXPECT_EQ(backend.recorded_draw.index_count, 3U);
    }

    // Validates shared DynamicMeshData renders as one instanced draw.
    TEST(RenderingTests, Render_SharedDynamicMeshDataBatchesAsInstancedDraw)
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
        auto mesh_data = std::make_shared<DynamicMeshData>(triangle);
        auto first = Entity("First", registry);
        first.add_component<DynamicMesh>(mesh_data);
        first.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto second = Entity("Second", registry);
        second.add_component<DynamicMesh>(mesh_data);
        second.add_component<Transform>(Vec3(1.0F, 0.0F, -2.0F));
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
            settings);
        rendering.render();
        wait_for_render_lane(thread_manager);

        // Assert
        ASSERT_FALSE(backend.recorded_draws.empty());
        EXPECT_EQ(backend.recorded_draws.front().instance_count, 2U);
        EXPECT_TRUE(contains_vertex_slot(backend, VERTEX_BUFFER_SLOT_INSTANCE));
        EXPECT_FALSE(mesh_data->is_dirty());
    }

    // Validates distinct dynamic mesh payloads do not batch just because contents match.
    TEST(RenderingTests, Render_DistinctDynamicMeshDataDoesNotBatch)
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
        auto first = Entity("First", registry);
        first.add_component<DynamicMesh>(triangle);
        first.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto second = Entity("Second", registry);
        second.add_component<DynamicMesh>(triangle);
        second.add_component<Transform>(Vec3(1.0F, 0.0F, -2.0F));
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
            settings);
        rendering.render();
        wait_for_render_lane(thread_manager);

        // Assert
        ASSERT_GE(backend.recorded_draws.size(), 2U);
        EXPECT_EQ(backend.recorded_draws[0U].instance_count, 1U);
        EXPECT_EQ(backend.recorded_draws[1U].instance_count, 1U);
    }

    // Validates dirty same-size dynamic mesh edits update cached buffers instead of reuploading.
    TEST(RenderingTests, Render_DirtyDynamicMeshSameSizeEditUpdatesCachedBuffers)
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
        auto mesh_data = std::make_shared<DynamicMeshData>(triangle);
        auto entity = Entity("DynamicTriangle", registry);
        entity.add_component<DynamicMesh>(mesh_data);
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
            settings);
        rendering.render();
        wait_for_render_lane(thread_manager);
        const uint dynamic_upload_count = count_dynamic_mesh_vertex_uploads(
            backend.recorded_buffer_uploads);
        const uint updated_buffer_count = backend.updated_buffer_count;

        // Act
        mesh_data->edit_mesh().vertices.vertices[0U] += 0.25F;
        rendering.render();
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_EQ(count_dynamic_mesh_vertex_uploads(backend.recorded_buffer_uploads), dynamic_upload_count);
        EXPECT_GE(backend.updated_buffer_count, updated_buffer_count + 2U);
        EXPECT_FALSE(mesh_data->is_dirty());
    }

    // Validates dirty size-changing dynamic mesh edits upload replacement buffers.
    TEST(RenderingTests, Render_DirtyDynamicMeshSizeChangeReuploadsBuffers)
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
        auto mesh_data = std::make_shared<DynamicMeshData>(triangle);
        auto entity = Entity("DynamicTriangle", registry);
        entity.add_component<DynamicMesh>(mesh_data);
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
            settings);
        rendering.render();
        wait_for_render_lane(thread_manager);
        const uint dynamic_upload_count = count_dynamic_mesh_vertex_uploads(
            backend.recorded_buffer_uploads);

        // Act
        mesh_data->edit_mesh().indices.push_back(0U);
        rendering.render();
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_GT(count_dynamic_mesh_vertex_uploads(backend.recorded_buffer_uploads), dynamic_upload_count);
        EXPECT_FALSE(mesh_data->is_dirty());
    }

}
