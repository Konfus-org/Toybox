#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/tbx_api.h"
#include "tbx/types/components/model.h"
#include "tbx/types/handle.h"
#include "tbx/types/material.h"
#include "tbx/types/shader.h"
#include "tbx/types/texture.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/utils/result.h"
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tbx
{
    inline constexpr uint32 TBX_MAX_MATERIAL_UNIFORM_VECTORS = 64U;

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
    /// Purpose: Describes one GPU resource cached from an asset handle.
    /// @details
    /// Ownership: Stores copied identifiers and usage counters; does not own backend resources.
    /// Thread Safety: Safe to copy between threads.
    struct TBX_API GraphicsResourceUsage
    {
        Handle asset = {};
        Uuid resource = {};
        uint access_count = 0U;
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
        std::vector<GraphicsResourceBinding> textures = {};
    };

    /// @brief
    /// Purpose: Loads commonly reused graphics assets and keeps their uploaded GPU resources hot.
    /// @details
    /// Ownership: Owns cache metadata and releases backend resources when evicted or destroyed.
    /// Thread Safety: Not inherently thread-safe; call from the graphics/backend thread.
    class TBX_API GraphicsResourceManager final
    {
      public:
        GraphicsResourceManager(
            IGraphicsBackend& backend,
            AssetManager& asset_manager,
            uint unused_frame_limit = 3U);
        ~GraphicsResourceManager() noexcept;

      public:
        GraphicsResourceManager(const GraphicsResourceManager&) = delete;
        GraphicsResourceManager& operator=(const GraphicsResourceManager&) = delete;
        GraphicsResourceManager(GraphicsResourceManager&&) noexcept = delete;
        GraphicsResourceManager& operator=(GraphicsResourceManager&&) noexcept = delete;

      public:
        /// @brief
        /// Purpose: Advances resource lifetime tracking and unloads stale resources.
        /// @details
        /// Ownership: Releases stale backend resources owned by this manager.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread once per frame.
        uint update();

        /// @brief
        /// Purpose: Returns true when an asset currently has an uploaded GPU resource.
        /// @details
        /// Ownership: Does not transfer ownership.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        bool is_loaded(const Handle& handle);

        /// @brief
        /// Purpose: Returns cached usage information for an asset when loaded.
        /// @details
        /// Ownership: Returns copied usage data.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        std::optional<GraphicsResourceUsage> get_usage(const Handle& handle);

        /// @brief
        /// Purpose: Loads a material asset, uploads its pipeline when needed, and returns it.
        /// @details
        /// Ownership: The returned UUID is owned by this manager until eviction or unload.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_material(const Handle& handle, Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Loads a material asset with custom parameters and returns the cached pipeline.
        /// @details
        /// Ownership: The returned UUID is owned by this manager until eviction or unload.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_material(
            const Handle& handle,
            const MaterialLoadParameters& parameters,
            Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Loads a material and returns default bindings merged with instance overrides.
        /// @details
        /// Ownership: Returned binding data is copied; use load_material_draw_resource for GPU
        /// bindings.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_material_instance(
            const MaterialInstance& instance,
            GraphicsMaterialInstanceResource& out_material_resource);

        /// @brief
        /// Purpose: Loads a material instance and returns renderer-ready draw bindings.
        /// @details
        /// Ownership: Returned binding data is copied; GPU resources are owned by this manager.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_material_draw_resource(
            const MaterialInstance& instance,
            GraphicsMaterialDrawResource& out_material_resource);

        /// @brief
        /// Purpose: Loads the renderer-owned fallback texture used for unassigned material maps.
        /// @details
        /// Ownership: The returned UUID is owned by this manager until unload_all or destruction.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_default_texture(Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Loads a material asset and returns the backend pipeline id as a raw uint.
        /// @details
        /// Ownership: The returned id is owned by this manager until eviction or unload.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_material(const Handle& handle, uint& out_gpu_handle);

        /// @brief
        /// Purpose: Loads a material asset with custom parameters and returns the raw backend id.
        /// @details
        /// Ownership: The returned id is owned by this manager until eviction or unload.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_material(
            const Handle& handle,
            const MaterialLoadParameters& parameters,
            uint& out_gpu_handle);

        /// @brief
        /// Purpose: Unloads one cached material pipeline resource.
        /// @details
        /// Ownership: Releases the backend resource owned by this manager.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        bool unload_material(const Handle& handle);

        /// @brief
        /// Purpose: Loads a model asset, uploads its mesh buffers when needed, and returns it.
        /// @details
        /// Ownership: The returned UUID identifies the manager-owned model GPU resource group.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_model(const Handle& handle, Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Loads a model asset with custom parameters and returns the cached resource.
        /// @details
        /// Ownership: The returned UUID identifies the manager-owned model GPU resource group.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_model(
            const Handle& handle,
            const ModelLoadParameters& parameters,
            Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Loads a model asset and returns the manager resource id as a raw uint.
        /// @details
        /// Ownership: The returned id is owned by this manager until eviction or unload.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_model(const Handle& handle, uint& out_gpu_handle);

        /// @brief
        /// Purpose: Loads a model asset with custom parameters and returns the raw resource id.
        /// @details
        /// Ownership: The returned id is owned by this manager until eviction or unload.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_model(
            const Handle& handle,
            const ModelLoadParameters& parameters,
            uint& out_gpu_handle);

        /// @brief
        /// Purpose: Loads a model asset and returns cached GPU draw metadata.
        /// @details
        /// Ownership: Returns copied metadata; model CPU data may be streamed out after upload.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_model(const Handle& handle, GraphicsModelResource& out_model_resource);

        /// @brief
        /// Purpose: Loads a model with custom parameters and returns cached GPU draw metadata.
        /// @details
        /// Ownership: Returns copied metadata; model CPU data may be streamed out after upload.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_model(
            const Handle& handle,
            const ModelLoadParameters& parameters,
            GraphicsModelResource& out_model_resource);

        /// @brief
        /// Purpose: Unloads one cached model resource group.
        /// @details
        /// Ownership: Releases all backend mesh buffer resources owned by this manager.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        bool unload_model(const Handle& handle);

        /// @brief
        /// Purpose: Loads a texture asset, uploads it when needed, and returns the cached resource.
        /// @details
        /// Ownership: The returned UUID is owned by this manager until eviction or unload.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_texture(const Handle& handle, Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Loads a texture asset with custom parameters and returns the cached resource.
        /// @details
        /// Ownership: The returned UUID is owned by this manager until eviction or unload.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_texture(
            const Handle& handle,
            const TextureLoadParameters& parameters,
            Uuid& out_resource_uuid);

        /// @brief
        /// Purpose: Loads a texture asset and returns the backend resource id as a raw uint.
        /// @details
        /// Ownership: The returned id is owned by this manager until eviction or unload.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_texture(const Handle& handle, uint& out_gpu_handle);

        /// @brief
        /// Purpose: Loads a texture asset with custom parameters and returns the raw backend id.
        /// @details
        /// Ownership: The returned id is owned by this manager until eviction or unload.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        Result load_texture(
            const Handle& handle,
            const TextureLoadParameters& parameters,
            uint& out_gpu_handle);

        /// @brief
        /// Purpose: Unloads one cached texture resource.
        /// @details
        /// Ownership: Releases the backend resource owned by this manager.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        bool unload_texture(const Handle& handle);

        /// @brief
        /// Purpose: Sets how many untouched frames a resource may remain cached.
        /// @details
        /// Ownership: Copies the provided frame count.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        void set_unused_frame_limit(uint unused_frame_limit);

        /// @brief
        /// Purpose: Unloads all cached GPU resources.
        /// @details
        /// Ownership: Releases backend resources owned by this manager.
        /// Thread Safety: Not thread-safe; call from the graphics/backend thread.
        void unload_all();

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
        void erase_usage(
            Uuid asset_id,
            std::unordered_map<Uuid, GraphicsResourceUsage>& resources,
            std::unordered_map<Uuid, uint>& last_access_frames);
        std::optional<GraphicsResourceUsage> find_usage(const Handle& handle);
        Result load_cached_material(
            const Handle& handle,
            const MaterialLoadParameters& parameters,
            Uuid& out_resource_uuid);
        Result load_cached_model(
            const Handle& handle,
            const ModelLoadParameters& parameters,
            Uuid& out_resource_uuid);
        Result load_cached_texture(
            const Handle& handle,
            const TextureLoadParameters& parameters,
            Uuid& out_resource_uuid);
        Result unload_backend_resources(Uuid asset_id, const GraphicsResourceUsage& usage);
        uint unload_unused(
            std::unordered_map<Uuid, GraphicsResourceUsage>& resources,
            std::unordered_map<Uuid, uint>& last_access_frames);
        Result upload_material_resource(
            const Handle& handle,
            const Material& material,
            Uuid& out_resource_uuid);
        Result load_fallback_material_resource(
            GraphicsMaterialInstanceResource& out_material_resource);
        Result load_material_textures(
            const MaterialTextureBindings& texture_bindings,
            std::vector<GraphicsResourceBinding>& out_textures);
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
        uint unload_unused();
        Uuid resolve_asset_id(const Handle& handle);

      private:
        IGraphicsBackend& _backend;
        AssetManager& _asset_manager;
        std::unordered_map<Uuid, GraphicsResourceUsage> _materials = {};
        std::unordered_map<Uuid, GraphicsMaterialInstanceResource> _material_resources = {};
        std::unordered_map<Uuid, uint> _material_last_access_frames = {};
        std::unordered_set<Uuid> _failed_materials = {};
        std::unordered_map<Uuid, GraphicsResourceUsage> _models = {};
        std::unordered_map<Uuid, GraphicsModelResource> _model_resources = {};
        std::unordered_map<Uuid, uint> _model_last_access_frames = {};
        std::unordered_map<Uuid, std::vector<Uuid>> _model_backend_resources = {};
        std::unordered_map<Uuid, GraphicsResourceUsage> _textures = {};
        std::unordered_map<Uuid, uint> _texture_last_access_frames = {};
        Uuid _default_texture = {};
        Uuid _default_normal_texture = {};
        Uuid _default_black_texture = {};
        Uuid _fallback_material_pipeline = {};
        GraphicsMaterialInstanceResource _fallback_material = {};
        uint _current_frame = 0U;
        uint _unused_frame_limit = 3U;
    };
}
