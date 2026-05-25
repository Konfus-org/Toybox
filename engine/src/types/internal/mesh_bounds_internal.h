#pragma once
#include "tbx/types/components/mesh.h"
#include "tbx/types/matrices.h"
#include "tbx/types/mesh_bounds.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace tbx::internal
{
    static bool try_get_position_attribute(
        const VertexBufferLayout& layout,
        uint32& out_stride_bytes,
        uint32& out_offset_bytes)
    {
        out_stride_bytes = layout.stride;
        out_offset_bytes = 0U;
        if (out_stride_bytes < static_cast<uint32>(sizeof(float) * 3U))
            return false;

        const uint32 attribute_count = static_cast<uint32>(layout.elements.size());
        for (uint32 attribute_index = 0U; attribute_index < attribute_count; ++attribute_index)
        {
            const auto& attribute = layout.elements[static_cast<size>(attribute_index)];
            if (attribute.debug_name != vertex_attribute_position_debug_name)
                continue;
            if (get_vertex_data_count(attribute.type) != 3
                || get_vertex_data_size(attribute.type) != static_cast<int32>(sizeof(float) * 3U))
                continue;

            out_offset_bytes = attribute.offset;
            return true;
        }

        return false;
    }

    static float get_max_scale_component(const Vec3& scale)
    {
        return std::max(std::abs(scale.x), std::max(std::abs(scale.y), std::abs(scale.z)));
    }

}
