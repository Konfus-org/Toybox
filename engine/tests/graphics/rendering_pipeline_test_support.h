#pragma once
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

    static const ShaderSceneUniforms* try_get_uploaded_frame_uniforms(
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
}
