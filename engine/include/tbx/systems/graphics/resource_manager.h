#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/graphics/graphics_resource_map.h"
#include "tbx/tbx_api.h"
#include "tbx/types/components/model.h"
#include "tbx/types/handle.h"
#include "tbx/types/material.h"
#include "tbx/types/shader.h"
#include "tbx/types/sphere.h"
#include "tbx/types/texture.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/utils/result.h"
#include <any>
#include <memory>
#include <optional>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace tbx
{
    inline constexpr uint32 TBX_MAX_MATERIAL_UNIFORM_VECTORS = 64U;

    /// @brief
    /// Purpose: Selects the pipeline shape used when uploading a material draw resource.
    /// @details
    /// Ownership: Value type only.
    /// Thread Safety: Safe to copy between threads.
    enum class GraphicsMaterialUploadMode : uint8_t
    {
        STANDARD = 0U,
        POST_PROCESS = 1U,
    };

    /// @brief
    /// Purpose: Stores packed material parameter values for the renderer uniform block.
    /// @details
    /// Ownership: Owns copied uniform values; safe to keep for command building.
    /// Thread Safety: Safe to copy between threads.
    struct TBX_API GraphicsMaterialUniformData
    {
        /// @brief
        /// Purpose: Returns a stable pointer to the packed uniform values.
        const void* data() const
        {
            return values.data();
        }

        /// @brief
        /// Purpose: Returns the packed uniform data size in bytes.
        uint64 byte_size() const
        {
            return static_cast<uint64>(values.size()) * static_cast<uint64>(sizeof(Vec4));
        }

        std::vector<Vec4> values = {};
    };

    /// @brief
    /// Purpose: Describes one uploaded mesh within a cached model resource.
    /// @details
    /// Ownership: Stores backend resource identifiers and draw counts by value.
    /// Thread Safety: Safe to copy between threads.
    struct TBX_API GraphicsModelMeshResource
    {
        Uuid vertex_buffer = {};
        Uuid index_buffer = {};
        uint32 index_count = 0U;
        Sphere local_bounds = {};
        bool has_local_bounds = false;
    };

    /// @brief
    /// Purpose: Describes cached GPU resources needed to draw a model asset.
    /// @details
    /// Ownership: Stores copied identifiers and mesh draw metadata; does not own CPU model data.
    /// Thread Safety: Safe to copy between threads.
    struct TBX_API GraphicsModelResource
    {
        Handle asset = {};
        Uuid resource = {};
        std::vector<GraphicsModelMeshResource> meshes = {};
    };

    /// @brief
    /// Purpose: Stores merged material defaults and runtime overrides for one draw.
    /// @details
    /// Ownership: Owns copied binding data; referenced GPU resources remain manager-owned.
    /// Thread Safety: Safe to copy between threads.
    struct TBX_API GraphicsMaterialInstanceResource
    {
        Handle material = {};
        Uuid pipeline = {};
        MaterialParameterBindings parameters = {};
        MaterialTextureBindings textures = {};
        MaterialConfig config = {};
    };

    /// @brief
    /// Purpose: Stores ready-to-bind material draw resources for one render item.
    /// @details
    /// Ownership: Owns copied binding data; backend resources remain manager-owned.
    /// Thread Safety: Safe to copy between threads.
    struct TBX_API GraphicsMaterialDrawResource
    {
        Uuid pipeline = {};
        uint64 uniform_key = 0U;
        GraphicsMaterialUniformData uniform_data = {};
        std::vector<std::string> parameter_names = {};
        std::vector<std::string> texture_names = {};
        std::vector<GraphicsResourceBinding> textures = {};
    };

    /// @brief
    /// Purpose: Uploads commonly reused graphics assets and keeps their GPU resources hot.
    /// @details
    /// Ownership: Owns cache metadata and releases backend resources when evicted or destroyed.
    /// Thread Safety: Not inherently thread-safe; call from the graphics/backend thread.
    class TBX_API GraphicsResourceManager final
    {
      public:
        GraphicsResourceManager(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager,
            uint unused_frame_limit = 3U);
        ~GraphicsResourceManager() noexcept;

      public:
        GraphicsResourceManager(const GraphicsResourceManager&) = delete;
        GraphicsResourceManager& operator=(const GraphicsResourceManager&) = delete;
        GraphicsResourceManager(GraphicsResourceManager&&) noexcept = delete;
        GraphicsResourceManager& operator=(GraphicsResourceManager&&) noexcept = delete;

      public:
        /// @brief
        /// Purpose: Sets how many untouched frames a resource may remain cached.
        void set_unused_frame_limit(uint unused_frame_limit);

        /// @brief
        /// Purpose: Returns cached usage information for an asset when loaded.
        std::optional<GraphicsResourceUsage> get_usage(const Handle& handle);

        /// @brief
        /// Purpose: Returns true when an asset currently has an uploaded GPU resource.
        bool is_loaded(const Handle& handle);

        /// @brief
        /// Purpose: Uploads a material asset and returns its cached pipeline resource.
        Result upload(
            const Handle& handle,
            const MaterialLoadParameters& parameters,
            Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Uploads a material asset and returns the backend pipeline id as a raw uint.
        Result upload(
            const Handle& handle,
            const MaterialLoadParameters& parameters,
            uint& out_gpu_handle);

        /// @brief
        /// Purpose: Uploads a material and returns default bindings merged with instance overrides.
        Result upload(
            const MaterialInstance& instance,
            GraphicsMaterialInstanceResource& out_material_resource);

        /// @brief
        /// Purpose: Uploads a material instance and returns renderer-ready draw bindings.
        Result upload(
            const MaterialInstance& instance,
            GraphicsMaterialDrawResource& out_material_resource,
            GraphicsMaterialUploadMode mode = GraphicsMaterialUploadMode::STANDARD);

        /// @brief
        /// Purpose: Uploads a model asset and returns its cached resource group id.
        Result upload(
            const Handle& handle,
            const ModelLoadParameters& parameters,
            Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Uploads a model asset and returns its cached resource group id as a raw uint.
        Result upload(
            const Handle& handle,
            const ModelLoadParameters& parameters,
            uint& out_gpu_handle);

        /// @brief
        /// Purpose: Uploads a model asset and returns cached GPU draw metadata.
        Result upload(
            const Handle& handle,
            const ModelLoadParameters& parameters,
            GraphicsModelResource& out_model_resource);

        /// @brief
        /// Purpose: Uploads caller-owned mesh data and returns cached GPU draw metadata.
        Result upload(
            const Handle& handle,
            const Mesh& mesh,
            GraphicsModelResource& out_model_resource);

        /// @brief
        /// Purpose: Uploads a runtime-owned GPU buffer through the manager.
        Result upload(
            const GraphicsBufferDesc& desc,
            const void* data,
            uint64 data_size,
            Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Uploads a runtime-owned GPU pipeline through the manager.
        Result upload(const GraphicsPipelineDesc& desc, Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Uploads a runtime-owned GPU sampler through the manager.
        Result upload(const GraphicsSamplerDesc& desc, Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Uploads a texture asset and returns its cached resource.
        Result upload(
            const Handle& handle,
            const TextureLoadParameters& parameters,
            Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Uploads a texture asset and returns the backend resource id as a raw uint.
        Result upload(
            const Handle& handle,
            const TextureLoadParameters& parameters,
            uint& out_gpu_handle);

        /// @brief
        /// Purpose: Uploads a runtime-owned GPU texture through the manager.
        Result upload(
            const GraphicsTextureDesc& desc,
            const void* data,
            uint64 data_size,
            Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Marks one runtime-owned GPU resource as used for this frame.
        Result update(const Uuid& resource_uuid);

        /// @brief
        /// Purpose: Updates one runtime-owned GPU buffer through the manager.
        Result update(
            const Uuid& resource_uuid,
            const void* data,
            uint64 data_size,
            uint64 offset);

        /// @brief
        /// Purpose: Updates one runtime-owned GPU texture through the manager.
        Result update(
            const Uuid& resource_uuid,
            const GraphicsTextureUpdateDesc& desc,
            const void* data,
            uint64 data_size);

        /// @brief
        /// Purpose: Unloads every manager-owned resource associated with an asset handle.
        Result unload(const Handle& handle);

        /// @brief
        /// Purpose: Unloads one manager-owned runtime GPU resource.
        Result unload(const Uuid& resource_uuid);

        /// @brief
        /// Purpose: Advances resource lifetime tracking and unloads stale resources.
        uint unload_stale();

      private:
        static std::vector<Pixel> expand_rgb_to_rgba(const Texture& texture);
        static GraphicsBufferDesc make_model_index_buffer_desc(
            const Handle& handle,
            uint mesh_index,
            uint64 byte_size);
        static GraphicsBufferDesc make_model_vertex_buffer_desc(
            const Handle& handle,
            uint mesh_index,
            uint64 byte_size);
        static GraphicsPipelineDesc make_material_pipeline_desc(
            const Material& material,
            Shader shader,
            const Handle& handle);
        static GraphicsPipelineDesc make_post_process_pipeline_desc(
            Shader shader,
            const Handle& handle);
        static void append_parameter_uniform_data(
            const MaterialParameterData& parameter,
            std::vector<Vec4>& out_values);
        static uint64 make_material_key(
            Uuid pipeline,
            const GraphicsMaterialUniformData& uniforms,
            const std::vector<GraphicsResourceBinding>& textures);
        static GraphicsMaterialUniformData make_material_uniform_data(
            const MaterialParameterBindings& parameters);
        static uint64 get_texture_channel_count(TextureFormat format);
        static uint64 get_texture_pixel_count(const Texture& texture);
        static uint64 get_texture_source_byte_size(const Texture& texture);
        static uint64 get_texture_upload_byte_size(const Texture& texture);
        static GraphicsTextureDesc make_texture_desc(const Texture& texture, const Handle& handle);

        bool append_shader_sources(
            const Handle& handle,
            std::vector<Uuid>& loaded_shader_ids,
            std::vector<ShaderSource>& shader_sources);
        Result build_material_shader(const Material& material, Shader& out_shader);
        static GraphicsResourceKey make_asset_resource_key(Uuid asset_id, uint32 bucket);
        static GraphicsResourceKey make_runtime_resource_key(Uuid resource_uuid);
        static bool should_pin_asset_for_bucket(uint32 bucket);
        void erase_usage(const GraphicsResourceKey& key);
        void pin_asset_if_tracked(const GraphicsResourceKey& key, const Handle& handle);
        void unpin_asset_if_unused(const GraphicsResourceKey& key, const Handle& handle);
        std::optional<GraphicsResourceUsage> find_usage(const Handle& handle);
        GraphicsResourceRecord* find_record(const GraphicsResourceKey& key);
        const GraphicsResourceRecord* find_record(const GraphicsResourceKey& key) const;
        std::shared_ptr<AssetManager> lock_asset_manager() const;
        std::shared_ptr<IGraphicsBackend> lock_backend() const;
        Result upload_cached_material(
            const Handle& handle,
            const MaterialLoadParameters& parameters,
            Uuid& out_resource_uuid);
        Result upload_cached_model(
            const Handle& handle,
            const ModelLoadParameters& parameters,
            Uuid& out_resource_uuid);
        Result upload_cached_texture(
            const Handle& handle,
            const TextureLoadParameters& parameters,
            Uuid& out_resource_uuid);
        Result unload_backend_resources(const GraphicsResourceRecord& record);
        Result unload_resource(const GraphicsResourceKey& key, const GraphicsResourceRecord& record);
        Result upload_material_resource(
            const Handle& handle,
            const Material& material,
            Uuid& out_resource_uuid);
        Result upload_post_process_material_resource(
            const Handle& handle,
            const Material& material,
            Uuid& out_resource_uuid);
        Result load_fallback_material_resource(
            GraphicsMaterialInstanceResource& out_material_resource);
        Result load_material_textures(
            const MaterialTextureBindings& texture_bindings,
            std::vector<GraphicsResourceBinding>& out_textures);
        Result load_default_texture(Uuid& out_resource_uuid);
        Result load_default_texture_for_binding(
            std::string_view binding_name,
            Uuid& out_resource_uuid);
        Result ensure_solid_fallback_texture(
            std::string_view debug_name,
            Pixel r,
            Pixel g,
            Pixel b,
            Pixel a,
            Uuid& out_resource_uuid);
        Result upload_model_resource(
            const Handle& handle,
            const Model& model,
            std::vector<Uuid>& out_backend_resources,
            std::vector<GraphicsModelMeshResource>& out_meshes,
            Uuid& out_resource_uuid);
        Result upload_texture_resource(
            const Handle& handle,
            const Texture& texture,
            Uuid& out_resource_uuid);
        void track_resource(
            const GraphicsResourceKey& key,
            const Handle& handle,
            Uuid resource_uuid,
            std::vector<Uuid> backend_resources = {},
            std::any payload = {});
        bool touch_resource(const GraphicsResourceKey& key);
        bool touch_runtime_resource(const Uuid& resource_uuid);
        Result upload_runtime_resource(
            const Handle& handle,
            Uuid& out_resource_uuid,
            const Result& upload_result);
        void unload_all();
        uint unload_unused();
        Uuid resolve_asset_id(const Handle& handle);

      private:
        std::weak_ptr<IGraphicsBackend> _backend = {};
        std::weak_ptr<AssetManager> _asset_manager = {};
        GraphicsResourceMap _resources = {};
        std::unordered_set<Uuid> _failed_materials = {};
        Uuid _default_texture = {};
        Uuid _default_normal_texture = {};
        Uuid _default_black_texture = {};
        Uuid _fallback_material_pipeline = {};
        GraphicsMaterialInstanceResource _fallback_material = {};
        uint _current_frame = 0U;
        uint _unused_frame_limit = 3U;
    };
}
