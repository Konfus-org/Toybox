#pragma once
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/systems/graphics/pipeline/commands/render_pass.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/tbx_api.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/size.h"
#include "tbx/types/uuid.h"
#include <array>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tbx
{
    class TBX_API LightingPassOperation final : public IRenderOperation
    {
      public:
        LightingPassOperation(
            std::weak_ptr<IGraphicsBackend> backend,
            GraphicsResourceManager& resource_manager);
        ~LightingPassOperation() noexcept override = default;
        RenderOperationDebugInfo get_debug_info() const override;
        Result prepare(RenderData& render_data) override;
        Result execute(
            IGraphicsBackend& backend,
            RenderData& render_data,
            const CancellationToken& token) override;

      private:
        static uint32 get_albedo_texture_slot();
        static uint32 get_normal_texture_slot();
        static uint32 get_emissive_texture_slot();
        static uint32 get_material_texture_slot();
        static uint32 get_depth_texture_slot();
        static uint32 get_directional_shadow_texture_slot();
        static uint32 get_point_shadow_texture_slot();
        static uint32 get_spot_shadow_texture_slot();
        static uint32 get_area_shadow_texture_slot();
        static uint32 get_material_uniform_slot();
        static uint32 get_lighting_info_uniform_slot();
        static uint32 get_point_lights_storage_slot();
        static uint32 get_spot_lights_storage_slot();
        static uint32 get_tile_light_spans_storage_slot();
        static uint32 get_tile_point_light_indices_storage_slot();
        static uint32 get_tile_spot_light_indices_storage_slot();
        static uint32 get_area_lights_storage_slot();
        static uint32 get_tile_area_light_indices_storage_slot();
        static uint32 get_directional_shadow_cascades_storage_slot();
        static uint32 get_spot_shadow_maps_storage_slot();
        static uint32 get_area_shadow_maps_storage_slot();
        static Vec4 make_light_radiance(const Light& light);
        static void set_texture_binding(
            std::vector<GraphicsResourceBinding>& bindings,
            uint32 slot,
            Uuid resource);
        Result ensure_material_uniform_buffer(
            IGraphicsBackend& backend,
            uint64 material_key,
            const void* data,
            uint64 data_size,
            Uuid& out_buffer);
        Result ensure_lighting_buffers(
            IGraphicsBackend& backend,
            const RenderData& render_data);
        Result ensure_render_targets(IGraphicsBackend& backend, const Size& resolution);

      private:
        std::weak_ptr<IGraphicsBackend> _backend;
        std::reference_wrapper<GraphicsResourceManager> _resource_manager;
        std::unordered_map<uint64, Uuid> _material_uniform_buffers = {};
        std::array<Uuid, 7U> _gbuffer_color_targets = {};
        Uuid _gbuffer_depth_target = {};
        Uuid _lighting_info_uniform_buffer = {};
        Uuid _point_lights_storage_buffer = {};
        Uuid _spot_lights_storage_buffer = {};
        Uuid _tile_light_spans_storage_buffer = {};
        Uuid _tile_point_light_indices_storage_buffer = {};
        Uuid _tile_spot_light_indices_storage_buffer = {};
        Uuid _area_lights_storage_buffer = {};
        Uuid _tile_area_light_indices_storage_buffer = {};
        Uuid _directional_shadow_cascades_storage_buffer = {};
        Uuid _spot_shadow_maps_storage_buffer = {};
        Uuid _area_shadow_maps_storage_buffer = {};
        uint64 _lighting_info_uniform_buffer_size = 0U;
        uint64 _point_lights_storage_buffer_size = 0U;
        uint64 _spot_lights_storage_buffer_size = 0U;
        uint64 _tile_light_spans_storage_buffer_size = 0U;
        uint64 _tile_point_light_indices_storage_buffer_size = 0U;
        uint64 _tile_spot_light_indices_storage_buffer_size = 0U;
        uint64 _area_lights_storage_buffer_size = 0U;
        uint64 _tile_area_light_indices_storage_buffer_size = 0U;
        uint64 _directional_shadow_cascades_storage_buffer_size = 0U;
        uint64 _spot_shadow_maps_storage_buffer_size = 0U;
        uint64 _area_shadow_maps_storage_buffer_size = 0U;
        Size _target_resolution = {};
    };

}
