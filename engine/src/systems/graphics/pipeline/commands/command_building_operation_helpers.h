#pragma once
#include "tbx/systems/graphics/api.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/graphics/pipeline/render_operation.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/uuid.h"
#include "tbx/utils/hash.h"
#include <memory>
#include <vector>

namespace tbx
{
    inline constexpr uint32 TBX_MAX_DIRECTIONAL_LIGHTS = 4U;
    inline constexpr uint32 TBX_MAX_POINT_LIGHTS = 16U;
    inline constexpr uint32 TBX_MAX_SPOT_LIGHTS = 8U;
    inline constexpr uint32 TBX_MAX_AREA_LIGHTS = 4U;
    inline constexpr uint32 TBX_SHADOW_CASCADE_COUNT = 3U;
    inline constexpr uint32 TBX_MAX_POINT_SHADOWS = TBX_MAX_POINT_LIGHTS;
    inline constexpr uint32 TBX_MAX_SPOT_SHADOWS = TBX_MAX_SPOT_LIGHTS;
    inline constexpr uint32 TBX_MAX_AREA_SHADOWS = TBX_MAX_AREA_LIGHTS;

    inline bool can_render_mesh_directly(const Mesh& mesh)
    {
        constexpr uint32 model_pipeline_stride = 16U;
        const uint32 stride_bytes = mesh.vertices.layout.stride;
        const uint32 stride = stride_bytes == 0U
                                  ? model_pipeline_stride
                                  : stride_bytes / static_cast<uint32>(sizeof(float));
        return !mesh.vertices.empty() && !mesh.indices.empty()
               && stride == model_pipeline_stride;
    }

    inline uint64 make_mesh_cache_key(const std::shared_ptr<Mesh>& mesh_data)
    {
        return reinterpret_cast<uint64>(mesh_data.get());
    }

    inline uint64 make_dynamic_batch_key(const uint64 mesh_key, const uint64 material_key)
    {
        return fnv1a_hash_value(material_key, fnv1a_hash_value(mesh_key, TBX_FNV1A_OFFSET_BASIS));
    }

    inline uint64 make_static_batch_key(
        const Uuid vertex_buffer,
        const Uuid index_buffer,
        const uint32 index_count,
        const uint64 material_key)
    {
        uint64 hash = fnv1a_hash_uuid(vertex_buffer, TBX_FNV1A_OFFSET_BASIS);
        hash = fnv1a_hash_uuid(index_buffer, hash);
        hash = fnv1a_hash_value(index_count, hash);
        hash = fnv1a_hash_value(material_key, hash);
        return hash == 0U ? 1U : hash;
    }

    inline Vec3 make_light_direction(const Transform& transform)
    {
        return glm::normalize(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
    }

}
