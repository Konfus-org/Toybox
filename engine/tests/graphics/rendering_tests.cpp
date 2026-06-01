#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/types/assets/builtin_assets.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/shader.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/post_processing.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include <chrono>
#include <cstring>
#include <future>
#include <thread>

namespace tbx::tests::graphics
{
    enum class GraphicsBackendCallback
    {
        BEGIN_FRAME,
        BEGIN_PASS,
        END_PASS,
        BIND_PIPELINE,
        BIND_GROUP,
        DRAW,
        DRAW_INDIRECT,
        DISPATCH_COMPUTE,
        PIPELINE_BARRIER,
        PRESENT,
        END_FRAME,
        WAIT_FOR_IDLE,
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
            callbacks.push_back(GraphicsBackendCallback::BEGIN_FRAME);
            return {};
        }

        Result begin_render_pass(const GraphicsRenderPassDesc& pass) override
        {
            recorded_pass = pass;
            recorded_viewport = pass.viewport.dimensions;
            recorded_passes.push_back(pass);
            active_render_pass_name = pass.debug_name;
            callbacks.push_back(GraphicsBackendCallback::BEGIN_PASS);
            return {};
        }

        Result bind_group(uint32, const Uuid& group_resource_uuid) override
        {
            recorded_bind_group = group_resource_uuid;
            recorded_bind_groups.push_back(group_resource_uuid);
            recorded_bind_group_passes.push_back(active_render_pass_name);
            if (const auto group = recorded_bind_group_descs.find(group_resource_uuid);
                group != recorded_bind_group_descs.end())
            {
                for (const auto& binding : group->second.bindings)
                {
                    recorded_bind_group_slots.push_back(binding.binding_slot);
                    recorded_vertex_slots.push_back(binding.binding_slot);
                    recorded_uniform_slots.push_back(binding.binding_slot);
                    recorded_texture_slots.push_back(binding.binding_slot);
                    recorded_bound_slots_by_pass.push_back(
                        {active_render_pass_name, binding.binding_slot});
                }
            }
            callbacks.push_back(GraphicsBackendCallback::BIND_GROUP);
            return {};
        }

        Result bind_compute_pipeline(const Uuid& pipeline_resource_uuid) override
        {
            recorded_pipeline = pipeline_resource_uuid;
            recorded_pipelines.push_back(pipeline_resource_uuid);
            callbacks.push_back(GraphicsBackendCallback::BIND_PIPELINE);
            return {};
        }

        Result bind_raster_pipeline(const Uuid& pipeline_resource_uuid) override
        {
            recorded_pipeline = pipeline_resource_uuid;
            recorded_pipelines.push_back(pipeline_resource_uuid);
            recorded_pipeline_passes.push_back(active_render_pass_name);
            callbacks.push_back(GraphicsBackendCallback::BIND_PIPELINE);
            return {};
        }

        Result draw(
            uint32 index_count,
            uint32 instance_count,
            uint32 first_index,
            int32 vertex_offset,
            uint32 first_instance) override
        {
            recorded_draw = GraphicsDrawIndexedDesc {
                .index_count = index_count,
                .index_offset = first_index,
                .vertex_offset = vertex_offset,
                .instance_count = instance_count,
                .first_instance = first_instance,
            };
            recorded_draws.push_back(recorded_draw);
            callbacks.push_back(GraphicsBackendCallback::DRAW);
            return {};
        }

        Result draw_indirect(const Uuid& argument_buffer, uint64, uint32 draw_count, uint32)
            override
        {
            recorded_indirect_argument_buffer = argument_buffer;
            recorded_indirect_draw_count = draw_count;
            callbacks.push_back(GraphicsBackendCallback::DRAW_INDIRECT);
            return {};
        }

        Result dispatch_compute(uint32 group_count_x, uint32 group_count_y, uint32 group_count_z)
            override
        {
            recorded_dispatch_groups = UVec3(group_count_x, group_count_y, group_count_z);
            callbacks.push_back(GraphicsBackendCallback::DISPATCH_COMPUTE);
            return {};
        }

        Result begin_compute_pass(const GraphicsComputePassDesc& pass) override
        {
            recorded_compute_pass_name = pass.debug_name;
            return {};
        }

        Result end_frame() override
        {
            callbacks.push_back(GraphicsBackendCallback::END_FRAME);
            return {};
        }

        Result end_compute_pass() override
        {
            return {};
        }

        Result end_render_pass() override
        {
            active_render_pass_name.clear();
            callbacks.push_back(GraphicsBackendCallback::END_PASS);
            return {};
        }

        GraphicsApi get_api() const override
        {
            return GraphicsApi::OPEN_GL;
        }

        VsyncMode get_vsync() const override
        {
            return recorded_vsync_mode;
        }

        Result set_vsync(const VsyncMode mode) override
        {
            recorded_vsync_mode = mode;
            updated_vsync_count += 1U;
            return {};
        }

        Result present() override
        {
            callbacks.push_back(GraphicsBackendCallback::PRESENT);
            return {};
        }

        Result destroy_resource(const Uuid& resource_uuid) override
        {
            unloaded_resources.push_back(resource_uuid);
            return {};
        }

        Result pipeline_barrier(const std::vector<PipelineBarrierDesc>& barriers) override
        {
            recorded_barriers.insert(recorded_barriers.end(), barriers.begin(), barriers.end());
            callbacks.push_back(GraphicsBackendCallback::PIPELINE_BARRIER);
            return {};
        }

        Result create_buffer(const GraphicsBufferDesc& desc, Uuid& out_resource_uuid) override
        {
            recorded_buffer_descs.push_back(desc);
            uploaded_buffer_count += 1U;
            out_resource_uuid = Uuid(next_uploaded_resource++);
            recorded_buffer_descs_by_resource[out_resource_uuid] = desc;
            return {};
        }

        Result create_bind_group(const BindGroupDesc& desc, Uuid& out_resource_uuid) override
        {
            out_resource_uuid = Uuid(next_uploaded_resource++);
            recorded_bind_group_descs[out_resource_uuid] = desc;
            uploaded_bind_group_count += 1U;
            return {};
        }

        Result create_bind_group_layout(const BindGroupLayoutDesc& desc, Uuid& out_resource_uuid)
            override
        {
            out_resource_uuid = Uuid(next_uploaded_resource++);
            recorded_bind_group_layout_descs[out_resource_uuid] = desc;
            return {};
        }

        Result create_compute_pipeline(const ComputePipelineDesc& desc, Uuid& out_resource_uuid)
            override
        {
            recorded_compute_pipeline_desc = desc;
            out_resource_uuid = Uuid(next_uploaded_resource++);
            return {};
        }

        Result create_raster_pipeline(const RasterPipelineDesc& desc, Uuid& out_resource_uuid)
            override
        {
            recorded_pipeline_desc = desc;
            recorded_pipeline_descs.push_back(desc);
            uploaded_pipeline_count += 1U;
            out_resource_uuid = Uuid(next_uploaded_resource++);
            return {};
        }

        Result create_sampler(const GraphicsSamplerDesc&, Uuid& out_resource_uuid) override
        {
            out_resource_uuid = Uuid(next_uploaded_resource++);
            return {};
        }

        Result create_texture(const GraphicsTextureDesc& desc, Uuid& out_resource_uuid) override
        {
            recorded_texture_desc = desc;
            recorded_texture_descs.push_back(desc);
            uploaded_texture_count += 1U;
            out_resource_uuid = Uuid(next_uploaded_resource++);
            recorded_texture_descs_by_resource[out_resource_uuid] = desc;
            return {};
        }

        Result write_buffer(const Uuid& resource_uuid, const void* data, uint64 data_size, uint64)
            override
        {
            auto upload = RecordedBufferUpload();
            if (const auto desc_it = recorded_buffer_descs_by_resource.find(resource_uuid);
                desc_it != recorded_buffer_descs_by_resource.end())
            {
                upload.desc = desc_it->second;
            }
            if (data != nullptr && data_size > 0U)
            {
                const auto* bytes = static_cast<const uint8*>(data);
                upload.data.assign(bytes, bytes + static_cast<size>(data_size));
            }
            recorded_buffer_uploads.push_back(std::move(upload));
            updated_buffer_count += 1U;
            updated_buffers.push_back(resource_uuid);
            return {};
        }

        Result write_texture(
            const Uuid& resource_uuid,
            const GraphicsTextureUpdateDesc&,
            const void* data,
            uint64 data_size) override
        {
            recorded_texture_upload_data.clear();
            if (data != nullptr && data_size > 0U)
            {
                const auto* bytes = static_cast<const uint8*>(data);
                recorded_texture_upload_data.assign(bytes, bytes + static_cast<size>(data_size));
            }
            recorded_texture_upload_size = data_size;
            if (const auto desc_it = recorded_texture_descs_by_resource.find(resource_uuid);
                desc_it != recorded_texture_descs_by_resource.end())
            {
                recorded_texture_desc = desc_it->second;
            }
            return {};
        }

        void wait_for_idle() override
        {
            wait_for_idle_thread_id = std::this_thread::get_id();
            callbacks.push_back(GraphicsBackendCallback::WAIT_FOR_IDLE);
        }

      public:
        std::vector<GraphicsBackendCallback> callbacks = {};
        std::thread::id begin_frame_thread_id = {};
        Window recorded_output_window = {};
        Size recorded_viewport = {};
        Size recorded_scissor = {};
        GraphicsPassDesc recorded_pass = {};
        std::vector<GraphicsPassDesc> recorded_passes = {};
        Uuid recorded_pipeline = {};
        std::vector<Uuid> recorded_pipelines = {};
        Uuid recorded_bind_group = {};
        std::vector<Uuid> recorded_bind_groups = {};
        std::vector<std::string> recorded_bind_group_passes = {};
        Uuid recorded_vertex_buffer = {};
        std::vector<Uuid> recorded_vertex_buffers = {};
        Uuid recorded_index_buffer = {};
        Uuid recorded_uniform_buffer = {};
        Uuid recorded_texture = {};
        Uuid recorded_sampler = {};
        uint32 recorded_vertex_slot = 0U;
        std::vector<uint32> recorded_vertex_slots = {};
        uint32 recorded_uniform_slot = 0U;
        std::vector<uint32> recorded_uniform_slots = {};
        uint32 recorded_texture_slot = 0U;
        std::vector<uint32> recorded_texture_slots = {};
        uint32 recorded_sampler_slot = 0U;
        std::vector<uint32> recorded_bind_group_slots = {};
        std::vector<std::pair<std::string, uint32>> recorded_bound_slots_by_pass = {};
        GraphicsIndexType recorded_index_type = GraphicsIndexType::UINT32;
        GraphicsDrawIndexedDesc recorded_draw = {};
        std::vector<GraphicsDrawIndexedDesc> recorded_draws = {};
        RasterPipelineDesc recorded_pipeline_desc = {};
        ComputePipelineDesc recorded_compute_pipeline_desc = {};
        std::vector<RasterPipelineDesc> recorded_pipeline_descs = {};
        std::vector<std::string> recorded_pipeline_passes = {};
        std::vector<GraphicsBufferDesc> recorded_buffer_descs = {};
        std::vector<RecordedBufferUpload> recorded_buffer_uploads = {};
        GraphicsTextureDesc recorded_texture_desc = {};
        std::vector<GraphicsTextureDesc> recorded_texture_descs = {};
        std::vector<uint8> recorded_texture_upload_data = {};
        std::vector<Uuid> unloaded_resources = {};
        std::vector<PipelineBarrierDesc> recorded_barriers = {};
        std::unordered_map<Uuid, BindGroupDesc> recorded_bind_group_descs = {};
        std::unordered_map<Uuid, BindGroupLayoutDesc> recorded_bind_group_layout_descs = {};
        std::unordered_map<Uuid, GraphicsBufferDesc> recorded_buffer_descs_by_resource = {};
        std::unordered_map<Uuid, GraphicsTextureDesc> recorded_texture_descs_by_resource = {};
        Uuid recorded_indirect_argument_buffer = {};
        UVec3 recorded_dispatch_groups = UVec3(0U);
        std::string recorded_compute_pass_name = {};
        uint64 recorded_texture_upload_size = 0U;
        uint32 recorded_indirect_draw_count = 0U;
        uint uploaded_buffer_count = 0U;
        uint uploaded_bind_group_count = 0U;
        uint uploaded_pipeline_count = 0U;
        uint uploaded_texture_count = 0U;
        uint updated_buffer_count = 0U;
        uint updated_vsync_count = 0U;
        uint32 next_uploaded_resource = 1000U;
        std::vector<Uuid> updated_buffers = {};
        std::thread::id wait_for_idle_thread_id = {};
        std::string active_render_pass_name = {};
        VsyncMode recorded_vsync_mode = VsyncMode::OFF;
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
            promise.set_value(Result());
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
            if (!main_window.id.is_valid())
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
            return main_window.id.is_valid() && is_window_open;
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

    static GraphicsSettings make_graphics_settings(
        bool vsync = false,
        GraphicsApi api = GraphicsApi::OPEN_GL,
        Size resolution = Size {1280U, 720U},
        uint32 shadow_map_resolution = 2048U,
        float shadow_render_distance = 90.0F,
        float shadow_softness = 1.0F,
        float local_light_max_distance = 64.0F,
        float shadow_caster_max_distance = 96.0F)
    {
        return GraphicsSettings {
            .vsync_enabled = vsync ? VsyncMode::ON : VsyncMode::OFF,
            .graphics_api = api,
            .resolution = resolution,
            .shadow_map_resolution = shadow_map_resolution,
            .shadow_render_distance = shadow_render_distance,
            .shadow_softness = shadow_softness,
            .local_light_max_distance = local_light_max_distance,
            .shadow_caster_max_distance = shadow_caster_max_distance,
        };
    }

    static std::shared_ptr<World> load_test_world(
        SerializationRegistry& serialization_registry,
        AssetManager& asset_manager)
    {
        static uint32 next_world_id = 0x1000U;
        serialization_registry.register_loader<World>(
            [](const std::filesystem::path&,
               const DefaultAssetLoadParameters&,
               const AssetLoadMetadata&,
               World&)
            {
                return Result();
            });

        const uint32 world_id = next_world_id++;
        const Handle handle(
            "Tests/GraphicsWorld_" + std::to_string(world_id) + ".world",
            Uuid(world_id));
        auto world = asset_manager.load<World>(handle);
        asset_manager.set_pinned(handle, true);
        return world;
    }

    static std::shared_ptr<Material> make_test_material_with_texture(const Handle& texture_handle)
    {
        auto material = Material {};
        material.textures.set("albedo_map", texture_handle);
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

    static std::optional<ModelShaderData> find_object_shader_data(
        const std::vector<RecordedBufferUpload>& uploads)
    {
        for (const auto& upload : uploads)
        {
            if (upload.desc.debug_name != "Object Shader Data"
                || upload.data.size() < sizeof(ModelShaderData))
                continue;

            auto shader_data = ModelShaderData {};
            std::memcpy(&shader_data, upload.data.data(), sizeof(ModelShaderData));
            return shader_data;
        }

        return std::nullopt;
    }

    static std::vector<ModelShaderData> find_object_shader_data_uploads(
        const std::vector<RecordedBufferUpload>& uploads)
    {
        auto shader_data_uploads = std::vector<ModelShaderData> {};
        for (const auto& upload : uploads)
        {
            if (upload.desc.debug_name != "Object Shader Data"
                || upload.data.size() < sizeof(ModelShaderData))
            {
                continue;
            }

            auto shader_data = ModelShaderData {};
            std::memcpy(&shader_data, upload.data.data(), sizeof(ModelShaderData));
            shader_data_uploads.push_back(shader_data);
        }

        return shader_data_uploads;
    }

    static std::vector<std::vector<Vec4>> find_material_shader_data_uploads(
        const std::vector<RecordedBufferUpload>& uploads)
    {
        auto shader_data_uploads = std::vector<std::vector<Vec4>> {};
        for (const auto& upload : uploads)
        {
            if (upload.desc.debug_name != "Material Shader Data"
                || upload.data.size() < sizeof(Vec4))
            {
                continue;
            }

            auto shader_data = std::vector<Vec4>(upload.data.size() / sizeof(Vec4));
            std::memcpy(shader_data.data(), upload.data.data(), shader_data.size() * sizeof(Vec4));
            shader_data_uploads.push_back(std::move(shader_data));
        }

        return shader_data_uploads;
    }

    static std::optional<LightingShaderData> find_light_shader_data(
        const std::vector<RecordedBufferUpload>& uploads)
    {
        for (const auto& upload : uploads)
        {
            if (upload.desc.debug_name != "Light Shader Data"
                || upload.data.size() < sizeof(LightingShaderData))
                continue;

            auto shader_data = LightingShaderData {};
            std::memcpy(&shader_data, upload.data.data(), sizeof(LightingShaderData));
            return shader_data;
        }

        return std::nullopt;
    }

    static std::optional<ShadowShaderData> find_shadow_shader_data(
        const std::vector<RecordedBufferUpload>& uploads)
    {
        for (const auto& upload : uploads)
        {
            if (upload.desc.debug_name != "Shadow Shader Data"
                || upload.data.size() < sizeof(ShadowShaderData))
                continue;

            auto shader_data = ShadowShaderData {};
            std::memcpy(&shader_data, upload.data.data(), sizeof(ShadowShaderData));
            return shader_data;
        }

        return std::nullopt;
    }

    static Vec3 project_shadow_coord(const Mat4& light_view_projection, const Vec3& world_position)
    {
        Vec4 light_position = light_view_projection * Vec4(world_position, 1.0F);
        light_position /= std::abs(light_position.w) > 0.000001F ? light_position.w : 1.0F;
        return Vec3(light_position) * 0.5F + Vec3(0.5F);
    }

    static bool is_shadow_coord_in_bounds(const Vec3& coord)
    {
        return coord.x >= 0.0F && coord.x <= 1.0F && coord.y >= 0.0F && coord.y <= 1.0F
               && coord.z >= 0.0F && coord.z <= 1.0F;
    }

    static uint count_dynamic_mesh_vertex_uploads(const std::vector<GraphicsBufferDesc>& buffers)
    {
        auto count = uint {};
        for (const auto& buffer : buffers)
        {
            if (buffer.usage == GraphicsBufferUsage::VERTEX
                && buffer.debug_name.find("Vertices") != std::string::npos)
            {
                count += 1U;
            }
        }

        return count;
    }

    static uint count_buffer_uploads_containing(
        const std::vector<RecordedBufferUpload>& uploads,
        const std::string& debug_name)
    {
        auto count = uint {};
        for (const auto& upload : uploads)
        {
            if (upload.desc.debug_name.find(debug_name) != std::string::npos)
                count += 1U;
        }

        return count;
    }

    static uint count_pass_pipeline_binds(
        const RecordingGraphicsBackend& backend,
        const std::string& pass_name)
    {
        return static_cast<uint>(std::count(
            backend.recorded_pipeline_passes.begin(),
            backend.recorded_pipeline_passes.end(),
            pass_name));
    }

    static bool contains_bound_slot_in_pass(
        const RecordingGraphicsBackend& backend,
        const std::string& pass_name,
        const uint32 slot)
    {
        return std::find(
                   backend.recorded_bound_slots_by_pass.begin(),
                   backend.recorded_bound_slots_by_pass.end(),
                   std::pair<std::string, uint32> {pass_name, slot})
               != backend.recorded_bound_slots_by_pass.end();
    }

    static uint count_texture_uploads_named(
        const std::vector<GraphicsTextureDesc>& textures,
        const std::string& debug_name)
    {
        auto count = uint {};
        for (const auto& texture : textures)
        {
            if (texture.debug_name == debug_name)
                count += 1U;
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

    static bool contains_uniform_slot(const RecordingGraphicsBackend& backend, const uint32 slot)
    {
        return std::find(
                   backend.recorded_uniform_slots.begin(),
                   backend.recorded_uniform_slots.end(),
                   slot)
               != backend.recorded_uniform_slots.end();
    }

    static bool contains_texture_slot(const RecordingGraphicsBackend& backend, const uint32 slot)
    {
        return std::find(
                   backend.recorded_texture_slots.begin(),
                   backend.recorded_texture_slots.end(),
                   slot)
               != backend.recorded_texture_slots.end();
    }

    // Validates Rendering opens frame state and submits geometry through render().
    TEST(RenderingTests, Render_DelegatesFrameAndGeometryPassCommands)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto entity = world->create_entity("Triangle");
        entity.add_component<DynamicMesh>(Mesh::TRIANGLE);
        entity.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        const auto expected_callbacks = std::vector<GraphicsBackendCallback> {
            GraphicsBackendCallback::BEGIN_FRAME,
            GraphicsBackendCallback::BEGIN_PASS,
            GraphicsBackendCallback::BIND_PIPELINE,
            GraphicsBackendCallback::BIND_GROUP,
            GraphicsBackendCallback::DRAW,
            GraphicsBackendCallback::END_PASS,
            GraphicsBackendCallback::PRESENT,
            GraphicsBackendCallback::END_FRAME,
        };

        EXPECT_EQ(backend.recorded_output_window.id, window_manager.window.id);
        EXPECT_EQ(backend.recorded_viewport.width, window_manager.size.width);
        EXPECT_EQ(backend.recorded_viewport.height, window_manager.size.height);
        ASSERT_FALSE(backend.recorded_passes.empty());
        EXPECT_EQ(backend.recorded_passes[0U].clear_flags, GraphicsClearFlags::COLOR_DEPTH);
        EXPECT_EQ(backend.recorded_passes[0U].debug_name, "Toybox GBuffer Pass");
        for (const auto expected_callback : expected_callbacks)
        {
            EXPECT_NE(
                std::find(backend.callbacks.begin(), backend.callbacks.end(), expected_callback),
                backend.callbacks.end());
        }
    }

    TEST(RenderingTests, Render_DrawsEveryLoadedWorld)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto first_world = load_test_world(*serialization_registry, asset_manager);
        auto second_world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto first_entity = first_world->create_entity("FirstWorldTriangle");
        first_entity.add_component<DynamicMesh>(Mesh::TRIANGLE);
        first_entity.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto second_entity = second_world->create_entity("SecondWorldTriangle");
        second_entity.add_component<DynamicMesh>(Mesh::TRIANGLE);
        second_entity.add_component<Transform>(Vec3(1.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_EQ(
            std::count(
                backend.callbacks.begin(),
                backend.callbacks.end(),
                GraphicsBackendCallback::BEGIN_FRAME),
            2);
        EXPECT_EQ(
            std::count(
                backend.callbacks.begin(),
                backend.callbacks.end(),
                GraphicsBackendCallback::PRESENT),
            2);
        EXPECT_GE(backend.recorded_draws.size(), 2U);
    }

    TEST(RenderingTests, Render_SkipsRedundantPipelineBindWithinPass)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto first = world->create_entity("FirstTriangle");
        first.add_component<DynamicMesh>(Mesh::TRIANGLE);
        first.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto second = world->create_entity("SecondTriangle");
        second.add_component<DynamicMesh>(Mesh::TRIANGLE);
        second.add_component<Transform>(Vec3(1.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_EQ(count_pass_pipeline_binds(backend, "Toybox GBuffer Pass"), 1U);
        EXPECT_EQ(
            static_cast<uint>(std::count(
                backend.recorded_bind_group_passes.begin(),
                backend.recorded_bind_group_passes.end(),
                "Toybox GBuffer Pass")),
            2U);
    }

    // Validates render() returns before the submitted frame finishes on the render lane.
    TEST(RenderingTests, Render_ReturnsBeforeSubmittedFrameCompletes)
    {
        // Arrange
        auto allow_begin_frame = std::promise<void> {};
        auto backend = BlockingGraphicsBackend(allow_begin_frame.get_future().share());
        auto begin_frame_started = backend.take_begin_frame_started_future();
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto entity = world->create_entity("Triangle");
        entity.add_component<DynamicMesh>(Mesh::TRIANGLE);
        entity.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);

        // Act
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
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
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        const auto caller_thread_id = std::this_thread::get_id();
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        {
            auto rendering = Rendering(
                backend_service,
                asset_manager_service,
                thread_manager_service,
                window_manager_service);
            rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
            wait_for_render_lane(thread_manager);
        }

        // Assert
        EXPECT_NE(backend.begin_frame_thread_id, std::thread::id {});
        EXPECT_NE(backend.wait_for_idle_thread_id, std::thread::id {});
        EXPECT_NE(backend.begin_frame_thread_id, caller_thread_id);
        EXPECT_EQ(backend.begin_frame_thread_id, backend.wait_for_idle_thread_id);
        EXPECT_FALSE(thread_manager.has_lane("render"));
    }

    // Validates renderer does not enqueue a frame after the output window has closed.
    TEST(RenderingTests, Render_SkipsFrameWhenMainWindowIsClosed)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);

        // Act
        const bool closed = window_manager.close(window_manager.window);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_TRUE(closed);
        EXPECT_EQ(
            std::find(
                backend.callbacks.begin(),
                backend.callbacks.end(),
                GraphicsBackendCallback::BEGIN_FRAME),
            backend.callbacks.end());
    }

    // Validates graphics setting changes are cached by rendering before pipeline execution.
    TEST(RenderingTests, ReceiveMessage_UpdatesCachedGraphicsSettingsForNextFrame)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<MessageCoordinator>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = AppSettings {};
        settings.graphics = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        settings.graphics.local_light_max_distance = 1.0F;
        auto camera = world->create_entity("Camera");
        camera.add_component<Camera>();
        camera.add_component<Transform>(Vec3(0.0F));
        auto light = world->create_entity("LocalLight");
        light.add_component<PointLight>();
        light.add_component<Transform>(Vec3(0.0F, 0.0F, -10.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        // Act
        settings.graphics.vsync_enabled = VsyncMode::ON;
        settings.graphics.local_light_max_distance = 64.0F;
        wait_for_render_lane(thread_manager);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings.graphics);
        wait_for_render_lane(thread_manager);
        const auto light_data = find_light_shader_data(backend.recorded_buffer_uploads);

        // Assert
        EXPECT_EQ(backend.recorded_vsync_mode, VsyncMode::ON);
        EXPECT_EQ(backend.updated_vsync_count, 1U);
        ASSERT_TRUE(light_data.has_value());
        EXPECT_EQ(light_data->light_meta.x, 1);
    }

    // Validates local light extraction uses the configured camera distance limit.
    TEST(RenderingTests, Render_LocalLightsRespectConfiguredDistance)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(
            false,
            GraphicsApi::OPEN_GL,
            Size {1280U, 720U},
            2048U,
            90.0F,
            1.0F,
            4.0F);
        auto camera = world->create_entity("Camera");
        camera.add_component<Camera>();
        camera.add_component<Transform>(Vec3(0.0F));
        auto near_light = world->create_entity("NearLight");
        near_light.add_component<PointLight>();
        near_light.add_component<Transform>(Vec3(0.0F, 0.0F, -3.0F));
        auto far_light = world->create_entity("FarLight");
        far_light.add_component<PointLight>();
        far_light.add_component<Transform>(Vec3(0.0F, 0.0F, -5.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        const auto light_data = find_light_shader_data(backend.recorded_buffer_uploads);

        // Assert
        ASSERT_TRUE(light_data.has_value());
        EXPECT_EQ(light_data->light_meta.x, 1);
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
        auto bind_group = Uuid();
        const auto bind_group_desc = BindGroupDesc {
            .bindings =
                {
                    ResourceBinding {.binding_slot = 0U, .resource_handle = vertex_buffer},
                    ResourceBinding {.binding_slot = 0U, .resource_handle = index_buffer},
                    ResourceBinding {.binding_slot = 0U, .resource_handle = uniform_buffer},
                    ResourceBinding {.binding_slot = 1U, .resource_handle = texture},
                    ResourceBinding {.binding_slot = 1U, .resource_handle = sampler},
                },
        };

        // Act
        auto result = backend.create_bind_group(bind_group_desc, bind_group);
        if (result)
            result = backend.bind_raster_pipeline(pipeline);
        if (result)
            result = backend.bind_group(0U, bind_group);
        if (result)
            result = backend.draw(36U, 1U, 0U, 0, 0U);

        // Assert
        const auto expected_callbacks = std::vector<GraphicsBackendCallback> {
            GraphicsBackendCallback::BIND_PIPELINE,
            GraphicsBackendCallback::BIND_GROUP,
            GraphicsBackendCallback::DRAW,
        };

        EXPECT_TRUE(result);
        EXPECT_EQ(backend.recorded_pipeline, pipeline);
        EXPECT_EQ(backend.recorded_bind_group, bind_group);
        EXPECT_EQ(backend.recorded_draw.index_count, 36U);
        EXPECT_EQ(backend.callbacks, expected_callbacks);
    }

    // Validates Rendering owns the Toybox geometry pass and submits render() commands.
    TEST(RenderingTests, Rendering_RenderSubmitsGeometryPass)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto entity = world->create_entity("Cube");
        entity.add_component<DynamicMesh>(Mesh::CUBE);
        entity.add_component<Transform>(Vec3(0.0F, 0.0F, -4.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        ASSERT_EQ(backend.recorded_passes.size(), 3U);
        EXPECT_EQ(backend.recorded_passes[0U].debug_name, "Toybox GBuffer Pass");
        EXPECT_EQ(backend.recorded_passes[0U].clear_flags, GraphicsClearFlags::COLOR_DEPTH);
        EXPECT_EQ(backend.recorded_passes[1U].debug_name, "Toybox Lighting Pass");
        EXPECT_EQ(backend.recorded_passes[1U].clear_flags, GraphicsClearFlags::COLOR);
        EXPECT_EQ(backend.recorded_passes[2U].debug_name, "Toybox Post Process Pass");
        EXPECT_EQ(backend.recorded_passes[2U].clear_flags, GraphicsClearFlags::COLOR);
        EXPECT_NE(
            std::find(
                backend.callbacks.begin(),
                backend.callbacks.end(),
                GraphicsBackendCallback::DRAW),
            backend.callbacks.end());
        EXPECT_TRUE(backend.unloaded_resources.empty());
    }

    // Validates render uniforms are built from ECS world transforms, not local transforms.
    TEST(RenderingTests, Render_UploadsWorldSpaceTransformUniforms)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto root = world->create_entity("Root");
        root.add_component<Transform>(Vec3(10.0F, 0.0F, 0.0F));
        auto camera = world->create_entity("Camera", root.get_id());
        camera.add_component<Camera>();
        camera.add_component<Transform>(Vec3(0.0F, 2.0F, 11.0F));
        auto mesh = world->create_entity("Mesh", root.get_id());
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        mesh.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
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
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto camera = world->create_entity("Camera");
        camera.add_component<Camera>();
        camera.add_component<Transform>(Vec3(0.0F, 0.0F, 5.0F));
        auto sun = world->create_entity("Sun");
        sun.add_component<DirectionalLight>(Color(1.0F, 0.5F, 0.25F, 1.0F), 2.0F, 0.15F);
        sun.add_component<Transform>();
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        const auto light_shader_data = find_light_shader_data(backend.recorded_buffer_uploads);

        // Assert
        EXPECT_EQ(sizeof(ShaderLightData), 64U);
        EXPECT_EQ(offsetof(LightingShaderData, lights), 48U);
        ASSERT_TRUE(light_shader_data.has_value());
        EXPECT_EQ(light_shader_data->light_meta.x, 1);
        EXPECT_FLOAT_EQ(light_shader_data->ambient_color.x, 0.3F);
        EXPECT_FLOAT_EQ(light_shader_data->ambient_color.y, 0.15F);
        EXPECT_FLOAT_EQ(light_shader_data->ambient_color.z, 0.075F);
        EXPECT_FLOAT_EQ(light_shader_data->lights[0U].position_type.w, 0.0F);
        EXPECT_FLOAT_EQ(light_shader_data->lights[0U].color_intensity.w, 2.0F);
        EXPECT_EQ(light_shader_data->light_meta.y, DIRECTIONAL_SHADOW_CASCADE_COUNT);
        EXPECT_FLOAT_EQ(light_shader_data->lights[0U].params.z, 0.0F);
        EXPECT_FLOAT_EQ(
            light_shader_data->lights[0U].params.w,
            static_cast<float>(DIRECTIONAL_SHADOW_CASCADE_COUNT));
    }

    // Validates directional ambient uses average directional color scaled by summed ambient.
    TEST(RenderingTests, Render_DirectionalAmbientUsesAverageColorAndSummedAmbientIntensity)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto camera = world->create_entity("Camera");
        camera.add_component<Camera>();
        camera.add_component<Transform>(Vec3(0.0F, 0.0F, 5.0F));
        auto key = world->create_entity("Key");
        key.add_component<DirectionalLight>(Color(1.0F, 0.2F, 0.0F, 1.0F), 1.0F, 0.20F);
        key.add_component<Transform>();
        auto fill = world->create_entity("Fill");
        fill.add_component<DirectionalLight>(Color(0.0F, 0.8F, 1.0F, 1.0F), 1.0F, 0.10F);
        fill.add_component<Transform>();
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        const auto light_shader_data = find_light_shader_data(backend.recorded_buffer_uploads);

        // Assert
        ASSERT_TRUE(light_shader_data.has_value());
        EXPECT_EQ(light_shader_data->light_meta.x, 2);
        EXPECT_FLOAT_EQ(light_shader_data->ambient_color.x, 0.15F);
        EXPECT_FLOAT_EQ(light_shader_data->ambient_color.y, 0.15F);
        EXPECT_FLOAT_EQ(light_shader_data->ambient_color.z, 0.15F);
    }

    // Validates Sky entities submit a dedicated skybox pass before the geometry pass.
    TEST(RenderingTests, Render_SkyComponentSubmitsSkyboxPassBeforeGeometryPass)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        serialization_registry->register_loader<Shader>(
            [](const std::filesystem::path& asset_path,
               const ShaderLoadParameters&,
               const AssetLoadMetadata&,
               Shader& shader)
            {
                const bool is_vertex_shader = asset_path.extension() == ".vert";
                shader = Shader(
                    is_vertex_shader
                        ? "#version 450 core\nvoid main(){ gl_Position = vec4(0.0); }\n"
                        : "#version 450 core\nlayout(location=0) out vec4 c; void main(){ c = "
                          "vec4(1.0); }\n",
                    is_vertex_shader ? ShaderType::VERTEX : ShaderType::FRAGMENT);
                return Result();
            });
        serialization_registry->register_loader<Material>(
            [](const std::filesystem::path&,
               const MaterialLoadParameters&,
               const AssetLoadMetadata&,
               Material& material)
            {
                material.shader.vertex = Handle("Shaders/TexturedSky.vert");
                material.shader.fragment = Handle("Shaders/TexturedSky.frag");
                material.textures.set("skybox_texture", Handle {});
                material.textures.set("secondary_skybox_texture", Handle {});
                material.parameters.set("color", Color(0.25F, 0.5F, 1.0F, 1.0F));
                material.parameters.set("brightness", 1.0F);
                material.parameters.set("ambient_multiplier", 0.0F);
                material.parameters.set("blend_factor", 0.0F);
                return Result();
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto camera = world->create_entity("Camera");
        camera.add_component<Camera>();
        camera.add_component<Transform>(Vec3(3.0F, 4.0F, 5.0F));
        auto sky_entity = world->create_entity("Sky");
        auto sky_material = MaterialInstance(Handle("Materials/TexturedSky.mat"));
        sky_material.set_texture(TexturedSkyMaterial::SKYBOX_TEXTURE, Handle("Textures/Sky.png"));
        sky_material.set_parameter(TexturedSkyMaterial::COLOR, Color(0.25F, 0.5F, 1.0F, 1.0F));
        sky_material.set_parameter(TexturedSkyMaterial::BRIGHTNESS, 1.5F);
        auto sky = Sky {};
        sky.material = sky_material;
        sky_entity.add_component<Sky>(sky);
        sky_entity.add_component<Transform>(Vec3(0.0F), Quat(0.70710678F, 0.0F, 0.70710678F, 0.0F));
        auto mesh_entity = world->create_entity("Triangle");
        mesh_entity.add_component<DynamicMesh>(Mesh::TRIANGLE);
        mesh_entity.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        const auto object_shader_data_uploads =
            find_object_shader_data_uploads(backend.recorded_buffer_uploads);
        const auto material_shader_data_uploads =
            find_material_shader_data_uploads(backend.recorded_buffer_uploads);

        // Assert
        const auto sky_pass = std::find_if(
            backend.recorded_passes.begin(),
            backend.recorded_passes.end(),
            [](const GraphicsPassDesc& desc)
            {
                return desc.debug_name == "Toybox Skybox Pass";
            });
        const auto gbuffer_pass = std::find_if(
            backend.recorded_passes.begin(),
            backend.recorded_passes.end(),
            [](const GraphicsPassDesc& desc)
            {
                return desc.debug_name == "Toybox GBuffer Pass";
            });
        const auto lighting_pass = std::find_if(
            backend.recorded_passes.begin(),
            backend.recorded_passes.end(),
            [](const GraphicsPassDesc& desc)
            {
                return desc.debug_name == "Toybox Lighting Pass";
            });
        ASSERT_NE(sky_pass, backend.recorded_passes.end());
        ASSERT_NE(gbuffer_pass, backend.recorded_passes.end());
        ASSERT_NE(lighting_pass, backend.recorded_passes.end());
        EXPECT_EQ(sky_pass->clear_flags, GraphicsClearFlags::COLOR);
        EXPECT_EQ(gbuffer_pass->clear_flags, GraphicsClearFlags::COLOR_DEPTH);
        EXPECT_EQ(lighting_pass->clear_flags, GraphicsClearFlags::NONE);
        EXPECT_LT(
            std::distance(backend.recorded_passes.begin(), sky_pass),
            std::distance(backend.recorded_passes.begin(), gbuffer_pass));
        EXPECT_LT(
            std::distance(backend.recorded_passes.begin(), gbuffer_pass),
            std::distance(backend.recorded_passes.begin(), lighting_pass));
        EXPECT_NE(
            std::find(
                backend.callbacks.begin(),
                backend.callbacks.end(),
                GraphicsBackendCallback::DRAW),
            backend.callbacks.end());
        const auto sky_object_data = std::find_if(
            object_shader_data_uploads.begin(),
            object_shader_data_uploads.end(),
            [](const ModelShaderData& shader_data)
            {
                return shader_data.model[3].x == 0.0F && shader_data.model[3].y == 0.0F
                       && shader_data.model[3].z == 0.0F;
            });
        ASSERT_NE(sky_object_data, object_shader_data_uploads.end());
        EXPECT_FLOAT_EQ(sky_object_data->model[3].x, 0.0F);
        EXPECT_FLOAT_EQ(sky_object_data->model[3].y, 0.0F);
        EXPECT_FLOAT_EQ(sky_object_data->model[3].z, 0.0F);
        EXPECT_NEAR(std::abs(sky_object_data->model[0].z), 1.0F, 0.0001F);
        const auto sky_material_upload = std::find_if(
            material_shader_data_uploads.begin(),
            material_shader_data_uploads.end(),
            [](const std::vector<Vec4>& upload)
            {
                return upload.size() >= 4U && upload[0U].x == 0.25F && upload[0U].y == 0.5F
                       && upload[0U].z == 1.0F && upload[1U].x == 1.5F;
            });
        ASSERT_NE(sky_material_upload, material_shader_data_uploads.end());
    }

    TEST(RenderingTests, Render_LoadsNamedMaterialHandleWithInvalidSerializedId)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto named_material_load_count = uint {};
        serialization_registry->register_loader<Material>(
            [&named_material_load_count](
                const std::filesystem::path& path,
                const MaterialLoadParameters&,
                const AssetLoadMetadata&,
                Material& material)
            {
                if (path.filename() == "Named.mat")
                    named_material_load_count += 1U;

                material.parameters.set("albedo_color", Color(0.2F, 0.4F, 0.6F, 1.0F));
                return Result();
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto camera = world->create_entity("Camera");
        camera.add_component<Camera>();
        auto mesh_entity = world->create_entity("Triangle");
        mesh_entity.add_component<DynamicMesh>(Mesh::TRIANGLE);
        mesh_entity.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        mesh_entity.add_component<MaterialInstance>(
            MaterialInstance(Handle("Materials/Named.mat", Uuid())));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_EQ(named_material_load_count, 1U);
    }

    TEST(RenderingTests, Render_LoadsNamedStaticMeshHandleWithInvalidSerializedId)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto named_model_load_count = uint {};
        serialization_registry->register_loader<Model>(
            [&named_model_load_count](
                const std::filesystem::path& path,
                const ModelLoadParameters&,
                const AssetLoadMetadata&,
                Model& model)
            {
                if (path.filename() == "NamedModel.fbx")
                    named_model_load_count += 1U;

                model = Model(Mesh::TRIANGLE);
                return Result();
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto camera = world->create_entity("Camera");
        camera.add_component<Camera>();
        auto mesh_entity = world->create_entity("Triangle");
        mesh_entity.add_component<StaticMesh>(StaticMesh(Handle("Models/NamedModel.fbx", Uuid())));
        mesh_entity.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_EQ(named_model_load_count, 1U);
    }

    TEST(RenderingTests, Render_PostProcessUsesNamedEffectHandleWithInvalidSerializedId)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto named_post_material_load_count = uint {};
        serialization_registry->register_loader<Material>(
            [&named_post_material_load_count](
                const std::filesystem::path& path,
                const MaterialLoadParameters&,
                const AssetLoadMetadata&,
                Material& material)
            {
                if (path.filename() == "NamedPost.mat")
                    named_post_material_load_count += 1U;

                material.config = MaterialConfig {
                    .is_depth_test_enabled = false,
                    .is_depth_write_enabled = false,
                    .is_two_sided = true,
                    .depth_function = MaterialDepthFunction::ALWAYS,
                };
                return Result();
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto camera = world->create_entity("Camera");
        camera.add_component<Camera>();
        auto post_entity = world->create_entity("PostProcessing");
        auto post_processing = PostProcessing {};
        post_processing.effects = {
            PostProcessingEffect {
                .material = MaterialInstance(Handle("Materials/NamedPost.mat", Uuid())),
                .is_enabled = true,
                .blend = 1.0F,
            },
        };
        post_entity.add_component<PostProcessing>(post_processing);
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_EQ(named_post_material_load_count, 1U);
        EXPECT_NE(
            std::find(
                backend.recorded_pipeline_passes.begin(),
                backend.recorded_pipeline_passes.end(),
                "Toybox Post Process Pass"),
            backend.recorded_pipeline_passes.end());
    }

    TEST(RenderingTests, Render_PostProcessFallsBackToTonemapWhenNoEffectDraws)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto camera = world->create_entity("Camera");
        camera.add_component<Camera>();
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_NE(
            std::find(
                backend.recorded_pipeline_passes.begin(),
                backend.recorded_pipeline_passes.end(),
                "Toybox Post Process Pass"),
            backend.recorded_pipeline_passes.end());
    }

    // Validates scheduled asset cleanup keeps active materials resident between frames.
    TEST(RenderingTests, Render_ActiveMaterialSurvivesScheduledAssetCleanup)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto material_load_count = uint {};
        serialization_registry->register_loader<Material>(
            [&material_load_count](
                const std::filesystem::path& path,
                const MaterialLoadParameters&,
                const AssetLoadMetadata&,
                Material& material)
            {
                if (path.filename() == "Transient.mat")
                    material_load_count += 1U;

                material.parameters.set("albedo_color", Color(0.2F, 0.4F, 0.6F, 1.0F));
                return Result();
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        const auto material_handle = Handle("Materials/Transient.mat");
        auto entity = world->create_entity("MaterialTriangle");
        entity.add_component<DynamicMesh>(Mesh::TRIANGLE);
        entity.add_component<MaterialInstance>(MaterialInstance(material_handle));
        entity.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);

        // Act
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        asset_manager.update(DeltaTime {.seconds = 1.0, .milliseconds = 1000.0});
        const AssetUsage material_usage_after_scheduled_cleanup =
            asset_manager.get_usage<Material>(material_handle);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_EQ(material_load_count, 1U);
        EXPECT_EQ(material_usage_after_scheduled_cleanup.stream_state, AssetStreamState::LOADED);
        EXPECT_EQ(material_usage_after_scheduled_cleanup.ref_count, 0U);
        EXPECT_GE(backend.uploaded_pipeline_count, 1U);
    }

    // Validates static mesh rendering reuses uploaded model buffers after CPU asset cleanup.
    TEST(RenderingTests, Render_StaticMeshReusesUploadedModelBuffers)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto model_load_count = uint {};
        serialization_registry->register_loader<Model>(
            [&model_load_count](
                const std::filesystem::path&,
                const ModelLoadParameters&,
                const AssetLoadMetadata&,
                Model& model)
            {
                model_load_count += 1U;
                model = Model(Mesh::TRIANGLE);
                return Result();
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        const auto model_handle = Handle("Models/Triangle.fbx");
        auto entity = world->create_entity("StaticTriangle");
        auto static_mesh = StaticMesh {};
        static_mesh.handle = model_handle;
        entity.add_component<StaticMesh>(static_mesh);
        entity.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);

        // Act
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        asset_manager.unload_unreferenced();
        const AssetUsage usage_after_asset_cleanup = asset_manager.get_usage<Model>(model_handle);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        const uint uploaded_buffer_count_after_ring_warmup = backend.uploaded_buffer_count;
        const uint updated_buffer_count_after_ring_warmup = backend.updated_buffer_count;
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
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
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto mesh_data = std::make_shared<DynamicMeshData>(Mesh::TRIANGLE);
        auto first = world->create_entity("First");
        first.add_component<DynamicMesh>(mesh_data);
        first.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto second = world->create_entity("Second");
        second.add_component<DynamicMesh>(mesh_data);
        second.add_component<Transform>(Vec3(1.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
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
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto first = world->create_entity("First");
        first.add_component<DynamicMesh>(Mesh::TRIANGLE);
        first.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto second = world->create_entity("Second");
        second.add_component<DynamicMesh>(Mesh::TRIANGLE);
        second.add_component<Transform>(Vec3(1.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
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
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto mesh_data = std::make_shared<DynamicMeshData>(Mesh::TRIANGLE);
        auto entity = world->create_entity("DynamicTriangle");
        entity.add_component<DynamicMesh>(mesh_data);
        entity.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        const uint dynamic_upload_count =
            count_dynamic_mesh_vertex_uploads(backend.recorded_buffer_descs);
        const uint updated_buffer_count = backend.updated_buffer_count;

        // Act
        mesh_data->edit_mesh().vertices.vertices[0U] += 0.25F;
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_EQ(
            count_dynamic_mesh_vertex_uploads(backend.recorded_buffer_descs),
            dynamic_upload_count);
        EXPECT_GE(backend.updated_buffer_count, updated_buffer_count + 2U);
        EXPECT_FALSE(mesh_data->is_dirty());
    }

    // Validates dirty size-changing dynamic mesh edits upload replacement buffers.
    TEST(RenderingTests, Render_DirtyDynamicMeshSizeChangeReuploadsBuffers)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto mesh_data = std::make_shared<DynamicMeshData>(Mesh::TRIANGLE);
        auto entity = world->create_entity("DynamicTriangle");
        entity.add_component<DynamicMesh>(mesh_data);
        entity.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        const uint dynamic_upload_count =
            count_dynamic_mesh_vertex_uploads(backend.recorded_buffer_descs);

        // Act
        mesh_data->edit_mesh().indices.push_back(0U);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_GT(
            count_dynamic_mesh_vertex_uploads(backend.recorded_buffer_descs),
            dynamic_upload_count);
        EXPECT_FALSE(mesh_data->is_dirty());
    }

    // Validates static meshes with the same source and material submit as one instanced draw.
    TEST(RenderingTests, Render_SharedStaticMeshAndMaterialBatchesAsInstancedDraw)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        serialization_registry->register_loader<Model>(
            [](const std::filesystem::path&,
               const ModelLoadParameters&,
               const AssetLoadMetadata&,
               Model& model)
            {
                model = Model(Mesh::TRIANGLE);
                return Result();
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        const auto model_handle = Handle("Models/BatchedTriangle.fbx");
        auto first = world->create_entity("FirstStatic");
        auto first_mesh = StaticMesh {};
        first_mesh.handle = model_handle;
        first.add_component<StaticMesh>(first_mesh);
        first.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto second = world->create_entity("SecondStatic");
        auto second_mesh = StaticMesh {};
        second_mesh.handle = model_handle;
        second.add_component<StaticMesh>(second_mesh);
        second.add_component<Transform>(Vec3(1.0F, 0.0F, -2.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        ASSERT_FALSE(backend.recorded_draws.empty());
        EXPECT_EQ(backend.recorded_draws.front().instance_count, 2U);
        EXPECT_EQ(backend.recorded_draws.front().index_count, 3U);
    }

    // Validates alpha-blended material assets render after opaque scene lighting.
    TEST(RenderingTests, Render_AlphaBlendedMaterialUsesTransparentForwardPass)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        serialization_registry->register_loader<Material>(
            [](const std::filesystem::path& path,
               const MaterialLoadParameters&,
               const AssetLoadMetadata&,
               Material& material)
            {
                material.parameters.set("albedo_color", Color::WHITE);
                if (path.filename() == "Transparent.mat")
                {
                    material.config.is_depth_write_enabled = false;
                    material.config.blend_mode = MaterialBlendMode::ALPHA_BLEND;
                }

                return Result();
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto opaque = world->create_entity("OpaqueCube");
        opaque.add_component<DynamicMesh>(Mesh::CUBE);
        opaque.add_component<Transform>(Vec3(0.0F, 0.0F, -4.0F));
        auto transparent = world->create_entity("TransparentCube");
        transparent.add_component<DynamicMesh>(Mesh::CUBE);
        transparent.add_component<Transform>(Vec3(0.0F, 0.0F, -5.0F));
        transparent.add_component<MaterialInstance>(
            MaterialInstance(Handle("Materials/Transparent.mat")));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        ASSERT_EQ(backend.recorded_passes.size(), 4U);
        EXPECT_EQ(backend.recorded_passes[0U].debug_name, "Toybox GBuffer Pass");
        EXPECT_EQ(backend.recorded_passes[1U].debug_name, "Toybox Lighting Pass");
        EXPECT_EQ(backend.recorded_passes[2U].debug_name, "Toybox Transparent Forward Pass");
        EXPECT_EQ(backend.recorded_passes[3U].debug_name, "Toybox Post Process Pass");
        ASSERT_GE(backend.recorded_draws.size(), 3U);
        EXPECT_EQ(
            count_buffer_uploads_containing(
                backend.recorded_buffer_uploads,
                "Instance Shader Data Toybox/Instances"),
            2U);
        EXPECT_NE(
            std::find_if(
                backend.recorded_pipeline_descs.begin(),
                backend.recorded_pipeline_descs.end(),
                [](const RasterPipelineDesc& desc)
                {
                    return desc.is_blending_enabled && !desc.is_depth_write_enabled;
                }),
            backend.recorded_pipeline_descs.end());
    }

    TEST(RenderingTests, Render_TransparentForwardPassUsesPreparedDrawShadowBindings)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        serialization_registry->register_loader<Material>(
            [](const std::filesystem::path&,
               const MaterialLoadParameters&,
               const AssetLoadMetadata&,
               Material& material)
            {
                material.parameters.set("albedo_color", Color::WHITE);
                material.config.is_depth_write_enabled = false;
                material.config.blend_mode = MaterialBlendMode::ALPHA_BLEND;
                return Result();
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto transparent = world->create_entity("TransparentCube");
        transparent.add_component<DynamicMesh>(Mesh::CUBE);
        transparent.add_component<Transform>(Vec3(0.0F, 0.0F, -5.0F));
        transparent.add_component<MaterialInstance>(
            MaterialInstance(Handle("Materials/Transparent.mat")));
        auto sun = world->create_entity("Sun");
        sun.add_component<DirectionalLight>(DirectionalLight());
        sun.add_component<Transform>(Vec3(0.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_TRUE(contains_bound_slot_in_pass(
            backend,
            "Toybox Transparent Forward Pass",
            BINDING_SHADOW_PASS_DATA));
        EXPECT_TRUE(contains_bound_slot_in_pass(
            backend,
            "Toybox Transparent Forward Pass",
            BINDING_SHADOW_MAP));
        EXPECT_EQ(
            count_buffer_uploads_containing(
                backend.recorded_buffer_uploads,
                "Instance Shader Data Toybox/Instances"),
            1U);
    }

    // Validates material overrides keep otherwise identical static meshes in separate batches.
    TEST(RenderingTests, Render_StaticMeshMaterialOverridesSplitBatches)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        serialization_registry->register_loader<Model>(
            [](const std::filesystem::path&,
               const ModelLoadParameters&,
               const AssetLoadMetadata&,
               Model& model)
            {
                model = Model(Mesh::TRIANGLE);
                return Result();
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        const auto model_handle = Handle("Models/SplitTriangle.fbx");
        auto first_material = MaterialInstance(PbrMaterial::HANDLE);
        first_material.set_parameter("test_value", 1.0F);
        auto second_material = MaterialInstance(PbrMaterial::HANDLE);
        second_material.set_parameter("test_value", 2.0F);
        auto first = world->create_entity("FirstStatic");
        auto first_mesh = StaticMesh {};
        first_mesh.handle = model_handle;
        first.add_component<StaticMesh>(first_mesh);
        first.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        first.add_component<MaterialInstance>(first_material);
        auto second = world->create_entity("SecondStatic");
        auto second_mesh = StaticMesh {};
        second_mesh.handle = model_handle;
        second.add_component<StaticMesh>(second_mesh);
        second.add_component<Transform>(Vec3(1.0F, 0.0F, -2.0F));
        second.add_component<MaterialInstance>(second_material);
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        ASSERT_GE(backend.recorded_draws.size(), 2U);
        EXPECT_EQ(backend.recorded_draws[0U].instance_count, 1U);
        EXPECT_EQ(backend.recorded_draws[1U].instance_count, 1U);
    }

    // Validates shadow caster batching excludes materials that opt out of shadows.
    TEST(RenderingTests, Render_ShadowCasterBatchExcludesNonCastingMaterial)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        serialization_registry->register_loader<Model>(
            [](const std::filesystem::path&,
               const ModelLoadParameters&,
               const AssetLoadMetadata&,
               Model& model)
            {
                model = Model(Mesh::TRIANGLE);
                return Result();
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        const auto model_handle = Handle("Models/ShadowTriangle.fbx");
        auto light = world->create_entity("Sun");
        light.add_component<DirectionalLight>(DirectionalLight());
        light.add_component<Transform>(Vec3(0.0F));
        auto first = world->create_entity("FirstCaster");
        auto first_mesh = StaticMesh {};
        first_mesh.handle = model_handle;
        first.add_component<StaticMesh>(first_mesh);
        first.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto second = world->create_entity("SecondCaster");
        auto second_mesh = StaticMesh {};
        second_mesh.handle = model_handle;
        second.add_component<StaticMesh>(second_mesh);
        second.add_component<Transform>(Vec3(1.0F, 0.0F, -2.0F));
        auto no_shadow_config = MaterialConfig {};
        no_shadow_config.shadow_mode = ShadowMode::OFF;
        auto no_shadow_material = MaterialInstance(PbrMaterial::HANDLE);
        no_shadow_material.set_config(no_shadow_config);
        auto hidden = world->create_entity("NoShadow");
        auto hidden_mesh = StaticMesh {};
        hidden_mesh.handle = model_handle;
        hidden.add_component<StaticMesh>(hidden_mesh);
        hidden.add_component<Transform>(Vec3(2.0F, 0.0F, -2.0F));
        hidden.add_component<MaterialInstance>(no_shadow_material);
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        ASSERT_FALSE(backend.recorded_passes.empty());
        EXPECT_EQ(backend.recorded_passes.front().debug_name, "Toybox Shadow Pass");
        EXPECT_FALSE(backend.recorded_passes.front().is_color_write_enabled);
        ASSERT_FALSE(backend.recorded_draws.empty());
        EXPECT_EQ(backend.recorded_draws.front().instance_count, 2U);
        EXPECT_TRUE(contains_uniform_slot(backend, BINDING_SHADOW_PASS_DATA));
    }

    // Validates directional shadows get the first shadow slot before local lights.
    TEST(RenderingTests, Render_DirectionalShadowTakesPrecedenceOverLocalShadow)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto mesh = world->create_entity("Triangle");
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        mesh.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto sun = world->create_entity("Sun");
        sun.add_component<DirectionalLight>(DirectionalLight());
        sun.add_component<Transform>(Vec3(0.0F));
        auto local = world->create_entity("LocalLight");
        auto& point_light = local.add_component<PointLight>();
        point_light.cast_shadows = true;
        point_light.range = 8.0F;
        local.add_component<Transform>(Vec3(0.0F, 0.0F, -3.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        const auto light_data = find_light_shader_data(backend.recorded_buffer_uploads);

        // Assert
        ASSERT_TRUE(light_data.has_value());
        ASSERT_GE(light_data->light_meta.x, 2);
        const ShaderLightData* directional_data = nullptr;
        const ShaderLightData* point_data = nullptr;
        for (int index = 0; index < light_data->light_meta.x; ++index)
        {
            const auto& shader_light = light_data->lights[static_cast<size>(index)];
            if (static_cast<uint32>(shader_light.position_type.w) == SHADER_LIGHT_TYPE_DIRECTIONAL)
                directional_data = &shader_light;
            if (static_cast<uint32>(shader_light.position_type.w) == SHADER_LIGHT_TYPE_POINT)
                point_data = &shader_light;
        }

        ASSERT_NE(directional_data, nullptr);
        ASSERT_NE(point_data, nullptr);
        EXPECT_EQ(light_data->light_meta.y, DIRECTIONAL_SHADOW_CASCADE_COUNT + 1U);
        EXPECT_EQ(directional_data->params.z, 0.0F);
        EXPECT_EQ(directional_data->params.w, static_cast<float>(DIRECTIONAL_SHADOW_CASCADE_COUNT));
        EXPECT_EQ(point_data->params.z, static_cast<float>(DIRECTIONAL_SHADOW_CASCADE_COUNT));
        EXPECT_EQ(point_data->params.w, 1.0F);
        EXPECT_EQ(
            std::count_if(
                backend.recorded_passes.begin(),
                backend.recorded_passes.end(),
                [](const GraphicsPassDesc& pass)
                {
                    return pass.debug_name == "Toybox Shadow Pass";
                }),
            DIRECTIONAL_SHADOW_CASCADE_COUNT + 1U);
        ASSERT_FALSE(backend.recorded_texture_descs.empty());
        EXPECT_EQ(backend.recorded_texture_descs.front().debug_name, "Toybox Shadow Map");
        EXPECT_TRUE(backend.recorded_texture_descs.front().is_depth_comparison_enabled);
        EXPECT_GE(
            backend.recorded_texture_descs.front().array_layer_count,
            DIRECTIONAL_SHADOW_CASCADE_COUNT + 1U);
        EXPECT_TRUE(contains_texture_slot(backend, BINDING_SHADOW_MAP));
        EXPECT_TRUE(contains_uniform_slot(backend, BINDING_SHADOW_PASS_DATA));
    }

    TEST(RenderingTests, Render_ShadowBatchUploadsAreSharedAcrossShadowLayers)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto mesh = world->create_entity("Triangle");
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        mesh.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto sun = world->create_entity("Sun");
        sun.add_component<DirectionalLight>(DirectionalLight());
        sun.add_component<Transform>(Vec3(0.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_EQ(
            count_buffer_uploads_containing(
                backend.recorded_buffer_uploads,
                "Instance Shader Data Toybox/ShadowInstances"),
            1U);
        EXPECT_EQ(
            count_buffer_uploads_containing(backend.recorded_buffer_uploads, "Shadow Shader Data"),
            DIRECTIONAL_SHADOW_CASCADE_COUNT);
        EXPECT_EQ(
            std::count_if(
                backend.recorded_passes.begin(),
                backend.recorded_passes.end(),
                [](const GraphicsPassDesc& pass)
                {
                    return pass.debug_name == "Toybox Shadow Pass";
                }),
            DIRECTIONAL_SHADOW_CASCADE_COUNT);
    }

    // Validates off-frustum shadow casters are kept when they are inside the configured distance.
    TEST(RenderingTests, Render_ShadowCasterDistanceIncludesOffFrustumCaster)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        serialization_registry->register_loader<Model>(
            [](const std::filesystem::path&,
               const ModelLoadParameters&,
               const AssetLoadMetadata&,
               Model& model)
            {
                model = Model(Mesh::TRIANGLE);
                return Result();
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(
            false,
            GraphicsApi::OPEN_GL,
            Size {1280U, 720U},
            2048U,
            90.0F,
            1.0F,
            64.0F,
            32.0F);
        const auto model_handle = Handle("Models/ShadowDistanceTriangle.fbx");
        auto visible_mesh = world->create_entity("VisibleTriangle");
        visible_mesh.add_component<StaticMesh>(StaticMesh(model_handle));
        visible_mesh.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto off_frustum_mesh = world->create_entity("OffFrustumTriangle");
        off_frustum_mesh.add_component<StaticMesh>(StaticMesh(model_handle));
        off_frustum_mesh.add_component<Transform>(Vec3(20.0F, 0.0F, -2.0F));
        auto sun = world->create_entity("Sun");
        sun.add_component<DirectionalLight>(DirectionalLight());
        sun.add_component<Transform>(Vec3(0.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        ASSERT_FALSE(backend.recorded_passes.empty());
        ASSERT_FALSE(backend.recorded_draws.empty());
        EXPECT_EQ(backend.recorded_passes.front().debug_name, "Toybox Shadow Pass");
        EXPECT_EQ(backend.recorded_draws.front().instance_count, 2U);
    }

    // Validates shadow caster distance still excludes off-frustum casters beyond the configured
    // limit.
    TEST(RenderingTests, Render_ShadowCasterDistanceExcludesFarOffFrustumCaster)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        serialization_registry->register_loader<Model>(
            [](const std::filesystem::path&,
               const ModelLoadParameters&,
               const AssetLoadMetadata&,
               Model& model)
            {
                model = Model(Mesh::TRIANGLE);
                return Result();
            });
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(
            false,
            GraphicsApi::OPEN_GL,
            Size {1280U, 720U},
            2048U,
            90.0F,
            1.0F,
            64.0F,
            8.0F);
        const auto model_handle = Handle("Models/FarShadowDistanceTriangle.fbx");
        auto visible_mesh = world->create_entity("VisibleTriangle");
        visible_mesh.add_component<StaticMesh>(StaticMesh(model_handle));
        visible_mesh.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto far_mesh = world->create_entity("FarOffFrustumTriangle");
        far_mesh.add_component<StaticMesh>(StaticMesh(model_handle));
        far_mesh.add_component<Transform>(Vec3(20.0F, 0.0F, -2.0F));
        auto sun = world->create_entity("Sun");
        sun.add_component<DirectionalLight>(DirectionalLight());
        sun.add_component<Transform>(Vec3(0.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        ASSERT_FALSE(backend.recorded_passes.empty());
        ASSERT_FALSE(backend.recorded_draws.empty());
        EXPECT_EQ(backend.recorded_passes.front().debug_name, "Toybox Shadow Pass");
        EXPECT_EQ(backend.recorded_draws.front().instance_count, 1U);
    }

    // Validates shadow settings are passed to internal pipeline helpers in the expected order.
    TEST(RenderingTests, Render_ShadowShaderDataUsesConfiguredDirectionalDistance)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(
            false,
            GraphicsApi::OPEN_GL,
            Size {1280U, 720U},
            4096U,
            120.0F,
            3.0F);
        auto mesh = world->create_entity("Triangle");
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        mesh.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto sun = world->create_entity("Sun");
        sun.add_component<DirectionalLight>(DirectionalLight());
        sun.add_component<Transform>(Vec3(0.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        const auto shadow_data = find_shadow_shader_data(backend.recorded_buffer_uploads);

        // Assert
        ASSERT_TRUE(shadow_data.has_value());
        const float expected_first_cascade_far =
            120.0F / static_cast<float>(DIRECTIONAL_SHADOW_CASCADE_COUNT);
        EXPECT_FLOAT_EQ(shadow_data->shadow_extra_params[0].y, expected_first_cascade_far);
        ASSERT_FALSE(backend.recorded_texture_descs.empty());
        EXPECT_EQ(backend.recorded_texture_descs.front().size.width, 4096U);
        EXPECT_EQ(backend.recorded_texture_descs.front().size.height, 4096U);
    }

    // Validates directional cascades cover camera-distance space instead of only the view frustum.
    TEST(RenderingTests, Render_DirectionalShadowCascadeIncludesOffscreenCasterDistance)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto mesh = world->create_entity("Triangle");
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        mesh.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto sun = world->create_entity("Sun");
        sun.add_component<DirectionalLight>(DirectionalLight());
        sun.add_component<Transform>(Vec3(0.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        const auto shadow_data = find_shadow_shader_data(backend.recorded_buffer_uploads);

        // Assert
        ASSERT_TRUE(shadow_data.has_value());
        const Vec3 offscreen_caster_position = Vec3(20.0F, 0.0F, 0.0F);
        const Vec3 shadow_coord =
            project_shadow_coord(shadow_data->light_view_projections[0], offscreen_caster_position);
        EXPECT_TRUE(is_shadow_coord_in_bounds(shadow_coord));
    }

    // Validates shadow resources are not uploaded when no eligible light casts shadows.
    TEST(RenderingTests, Render_NoShadowedLightSkipsShadowPassAndShadowMapUpload)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto mesh = world->create_entity("Triangle");
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        mesh.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto sun_light = DirectionalLight();
        sun_light.cast_shadows = false;
        auto sun = world->create_entity("Sun");
        sun.add_component<DirectionalLight>(sun_light);
        sun.add_component<Transform>(Vec3(0.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);

        // Act
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_EQ(
            std::find_if(
                backend.recorded_passes.begin(),
                backend.recorded_passes.end(),
                [](const GraphicsPassDesc& pass)
                {
                    return pass.debug_name == "Toybox Shadow Pass";
                }),
            backend.recorded_passes.end());
        EXPECT_EQ(
            count_texture_uploads_named(backend.recorded_texture_descs, "Toybox Shadow Map"),
            0U);
        EXPECT_FALSE(contains_texture_slot(backend, BINDING_SHADOW_MAP));
        EXPECT_FALSE(contains_uniform_slot(backend, BINDING_SHADOW_PASS_DATA));
    }

    // Validates renderer-owned shadow map resources are reused after cache warmup.
    TEST(RenderingTests, Render_ShadowMapUploadReusesCachedRenderTarget)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto thread_manager = ThreadManager {};
        auto window_manager = RecordingWindowManager {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto world = load_test_world(*serialization_registry, asset_manager);
        auto settings = make_graphics_settings(false, GraphicsApi::OPEN_GL, Size {1280U, 720U});
        auto mesh = world->create_entity("Triangle");
        mesh.add_component<DynamicMesh>(Mesh::TRIANGLE);
        mesh.add_component<Transform>(Vec3(0.0F, 0.0F, -2.0F));
        auto sun = world->create_entity("Sun");
        sun.add_component<DirectionalLight>(DirectionalLight());
        sun.add_component<Transform>(Vec3(0.0F));
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto thread_manager_service = make_non_owning_service(thread_manager);
        auto window_manager_service = make_non_owning_service<IWindowManager>(window_manager);
        auto rendering = Rendering(
            backend_service,
            asset_manager_service,
            thread_manager_service,
            window_manager_service);
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);
        const uint shadow_upload_count =
            count_texture_uploads_named(backend.recorded_texture_descs, "Toybox Shadow Map");

        // Act
        rendering.render(DeltaTime {1.0 / 60.0, 16.666666666666668}, settings);
        wait_for_render_lane(thread_manager);

        // Assert
        EXPECT_EQ(
            count_texture_uploads_named(backend.recorded_texture_descs, "Toybox Shadow Map"),
            shadow_upload_count);
    }

    // Validates auto-unloaded dynamic mesh buffers are removed from the upload cache.
    TEST(RenderingTests, RenderingResourceManager_AutoUnloadedDynamicMeshResourceForcesReupload)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto resource_manager =
            RenderingResourceManager(backend_service, asset_manager_service, 0.0F);
        auto mesh_data = std::make_shared<DynamicMeshData>(Mesh::TRIANGLE);
        const auto first_mesh = resource_manager.upload_dynamic_mesh(mesh_data);
        const uint uploaded_buffer_count = backend.uploaded_buffer_count;

        // Act
        resource_manager.update(DeltaTime {});
        const auto second_mesh = resource_manager.upload_dynamic_mesh(mesh_data);

        // Assert
        EXPECT_TRUE(first_mesh.vertex_buffer.is_valid());
        EXPECT_TRUE(second_mesh.vertex_buffer.is_valid());
        EXPECT_FALSE(resource_manager.is_managed(first_mesh.vertex_buffer));
        EXPECT_TRUE(resource_manager.is_managed(second_mesh.vertex_buffer));
        EXPECT_NE(first_mesh.vertex_buffer, second_mesh.vertex_buffer);
        EXPECT_GT(backend.uploaded_buffer_count, uploaded_buffer_count);
        EXPECT_FALSE(mesh_data->is_dirty());
    }

    // Validates identical draw bind groups reuse the resource-manager-owned cache entry.
    TEST(RenderingTests, RenderingResourceManager_ReusesEquivalentBindGroups)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto resource_manager =
            RenderingResourceManager(backend_service, std::weak_ptr<AssetManager>());
        const auto desc = BindGroupDesc {
            .bindings =
                {
                    ResourceBinding {
                        .binding_slot = 0U,
                        .resource_handle = Uuid(10U),
                    },
                    ResourceBinding {
                        .binding_slot = 1U,
                        .resource_handle = Uuid(20U),
                        .offset = 16U,
                        .range = 64U,
                    },
                },
            .debug_name = "Test Bind Group",
        };
        // Act
        const Uuid first_bind_group = resource_manager.upload_bind_group(desc);
        const Uuid second_bind_group = resource_manager.upload_bind_group(desc);

        // Assert
        EXPECT_TRUE(first_bind_group.is_valid());
        EXPECT_EQ(first_bind_group, second_bind_group);
        EXPECT_EQ(backend.uploaded_bind_group_count, 1U);
        EXPECT_TRUE(resource_manager.is_managed(first_bind_group));
    }

    // Validates discarded referenced resources invalidate cached bind group lookup entries.
    TEST(RenderingTests, RenderingResourceManager_DiscardedBindGroupResourceForcesReupload)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto resource_manager =
            RenderingResourceManager(backend_service, std::weak_ptr<AssetManager>(), 0.0F);
        const auto referenced_buffer = Uuid(10U);
        const auto desc = BindGroupDesc {
            .bindings =
                {
                    ResourceBinding {
                        .binding_slot = 0U,
                        .resource_handle = referenced_buffer,
                    },
                },
            .debug_name = "Invalidation Test Bind Group",
        };
        const Uuid first_bind_group = resource_manager.upload_bind_group(desc);
        const uint uploaded_bind_group_count = backend.uploaded_bind_group_count;

        // Act
        resource_manager.update(DeltaTime {});
        const Uuid second_bind_group = resource_manager.upload_bind_group(desc);

        // Assert
        EXPECT_TRUE(first_bind_group.is_valid());
        EXPECT_TRUE(second_bind_group.is_valid());
        EXPECT_NE(first_bind_group, second_bind_group);
        EXPECT_GT(backend.uploaded_bind_group_count, uploaded_bind_group_count);
        EXPECT_FALSE(resource_manager.is_managed(first_bind_group));
        EXPECT_TRUE(resource_manager.is_managed(second_bind_group));
    }

    // Validates auto-unloaded instance vertex buffers are removed from the upload cache.
    TEST(RenderingTests, RenderingResourceManager_AutoUnloadedInstanceResourceForcesReupload)
    {
        // Arrange
        auto backend = RecordingGraphicsBackend {};
        auto dispatcher = std::make_shared<NullMessageDispatcher>();
        auto serialization_registry = std::make_shared<SerializationRegistry>();
        auto asset_manager =
            AssetManager(dispatcher, serialization_registry, std::filesystem::path {});
        auto backend_service = make_non_owning_service<IGraphicsBackend>(backend);
        auto asset_manager_service = make_non_owning_service(asset_manager);
        auto resource_manager =
            RenderingResourceManager(backend_service, asset_manager_service, 0.0F);
        auto instance = ModelShaderData {};
        const GraphicsResourceBinding first_binding = resource_manager.upload_instance_buffer(
            "Toybox/Test/Instances",
            0U,
            &instance,
            static_cast<uint64>(sizeof(instance)));
        const uint uploaded_buffer_count = backend.uploaded_buffer_count;

        // Act
        resource_manager.update(DeltaTime {});
        const GraphicsResourceBinding second_binding = resource_manager.upload_instance_buffer(
            "Toybox/Test/Instances",
            0U,
            &instance,
            static_cast<uint64>(sizeof(instance)));

        // Assert
        EXPECT_TRUE(first_binding.resource.is_valid());
        EXPECT_TRUE(second_binding.resource.is_valid());
        EXPECT_FALSE(resource_manager.is_managed(first_binding.resource));
        EXPECT_TRUE(resource_manager.is_managed(second_binding.resource));
        EXPECT_NE(first_binding.resource, second_binding.resource);
        EXPECT_GT(backend.uploaded_buffer_count, uploaded_buffer_count);
    }

}
