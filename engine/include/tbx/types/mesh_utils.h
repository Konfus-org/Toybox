#pragma once
#include "tbx/types/components/mesh.h"

namespace tbx
{
    TBX_API VertexBuffer make_vertex_buffer(const std::vector<Vertex>& vertices);
    TBX_API Vec3 get_fallback_tangent(const Vec3& normal);
    TBX_API void compute_tangents(std::vector<Vertex>& vertices, const IndexBuffer& indices);
    TBX_API Mesh make_uv_sphere_mesh(float radius, uint32 stacks, uint32 sectors);
    TBX_API Mesh make_capsule_mesh(
        float radius,
        float cylinder_half_height,
        uint32 hemisphere_stacks,
        uint32 cylinder_stacks,
        uint32 sectors);
}
