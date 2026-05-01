#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/render_pipeline.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/math/matrices.h"
#include "tbx/tbx_api.h"
#include "tbx/utils/result.h"
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tbx
{
    struct Mesh;

    /// @brief
    /// Purpose: Owns Toybox-side render submission against a graphics backend.
    /// @details
    /// Ownership: Owns render pipeline resources; borrows runtime services.
    /// Thread Safety: Not inherently thread-safe; callers should synchronize backend access.
    class TBX_API Rendering
    {
      public:
        Rendering(
            IGraphicsBackend& backend,
            EntityRegistry& entity_registry,
            AssetManager& asset_manager,
            IWindowManager& window_manager,
            Window output_window,
            const GraphicsSettings& settings);
        ~Rendering() noexcept;

      public:
        Rendering(const Rendering&) = delete;
        Rendering& operator=(const Rendering&) = delete;
        Rendering(Rendering&&) noexcept = delete;
        Rendering& operator=(Rendering&&) noexcept = delete;

      public:
        void render();

      private:
        Result begin_frame_and_view();
        Result end_view_and_frame();
        Result ensure_geometry_buffers(
            const std::vector<float>& vertices,
            const std::vector<uint32>& indices);
        Result ensure_geometry_pipeline();
        Result ensure_dynamic_mesh_instance_buffer(
            uint64 mesh_key,
            const std::vector<Mat4>& world_to_clip_transforms,
            Uuid& out_buffer);
        Result ensure_material_uniform_buffer(
            uint64 material_key,
            const void* material_data,
            uint64 material_data_size,
            Uuid& out_buffer);
        Result ensure_view_uniform_buffer(const Mat4& view_projection, Uuid& out_buffer);
        Result ensure_dynamic_mesh_pipeline();
        Result ensure_model_pipeline();
        Result ensure_dynamic_mesh_buffers(
            const std::shared_ptr<Mesh>& mesh_data,
            Uuid& out_vertex_buffer,
            Uuid& out_index_buffer,
            uint32& out_index_count);
        Result ensure_model_transform_buffer(
            Uuid entity_id,
            const Mat4& world_to_clip,
            Uuid& out_buffer);
        Size get_render_resolution() const;
        void release_resources();
        Result append_dynamic_mesh_draws(
            const Mat4& view_projection,
            const Uuid& view_uniform_buffer,
            std::vector<GraphicsIndexedDrawCommand>& out_draws,
            std::vector<float>& out_fallback_vertices,
            std::vector<uint32>& out_fallback_indices);
        Result append_static_model_draws(
            const Mat4& view_projection,
            const Uuid& view_uniform_buffer,
            std::vector<GraphicsIndexedDrawCommand>& out_draws);
        void setup_geometry_pass(
            uint32 index_count,
            std::vector<GraphicsIndexedDrawCommand> static_draws);
        void unload_stale_dynamic_mesh_buffers();
        void unload_stale_material_uniform_buffers();
        void unload_stale_model_transform_buffers();

      private:
        std::reference_wrapper<IGraphicsBackend> _backend;
        std::reference_wrapper<EntityRegistry> _entity_registry;
        std::reference_wrapper<IWindowManager> _window_manager;
        Window _output_window = {};
        Size _requested_resolution = {};
        std::unique_ptr<GraphicsResourceManager> _resource_manager = {};
        std::unique_ptr<GraphicsRenderPipeline> _pipeline = {};
        Uuid _geometry_index_buffer = {};
        Uuid _geometry_pipeline = {};
        Uuid _geometry_vertex_buffer = {};
        Uuid _dynamic_mesh_pipeline = {};
        Uuid _model_pipeline = {};
        uint64 _geometry_index_buffer_size = 0U;
        uint64 _geometry_vertex_buffer_size = 0U;
        std::unordered_map<uint64, Uuid> _dynamic_mesh_index_buffers = {};
        std::unordered_map<uint64, uint32> _dynamic_mesh_index_counts = {};
        std::unordered_map<uint64, Uuid> _dynamic_mesh_instance_buffers = {};
        std::unordered_map<uint64, uint64> _dynamic_mesh_instance_buffer_sizes = {};
        std::unordered_map<uint64, uint64> _dynamic_mesh_last_access_frames = {};
        std::unordered_map<uint64, std::shared_ptr<const Mesh>> _dynamic_mesh_sources = {};
        std::unordered_map<uint64, Uuid> _dynamic_mesh_vertex_buffers = {};
        std::unordered_map<uint64, Uuid> _material_uniform_buffers = {};
        std::unordered_map<uint64, uint64> _material_uniform_last_access_frames = {};
        std::unordered_map<Uuid, uint64> _model_transform_last_access_frames = {};
        std::unordered_map<Uuid, Uuid> _model_transform_buffers = {};
        Uuid _view_uniform_buffer = {};
        uint64 _render_frame = 0U;
        Result _initialization_result = {};
    };
}
