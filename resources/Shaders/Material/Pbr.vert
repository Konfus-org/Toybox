#include "ShaderBase.glsl"

// Forward+ standard vertex: pulls the instance + vertex straight from the global SSBOs (no vertex
// attributes are bound). gl_BaseInstance carries the visible instance index emitted by the cull
// shader; gl_VertexID indexes the global vertex mega-buffer (indices are stored globally).
layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;
layout(location = 2) out vec3 v_world_position;
layout(location = 3) out vec3 v_world_normal;
layout(location = 4) out vec4 v_world_tangent;
layout(location = 5) out flat uint v_material_id;

void main()
{
    InstanceData instance = instances[gl_BaseInstance];
    VertexData vertex = vertices[gl_VertexID];

    vec4 world_position = instance.modelMatrix * vec4(vertex.position.xyz, 1.0);
    mat3 normal_matrix = mat3(instance.modelMatrix);

    v_uv = vertex.uv.xy;
    v_color = vertex.color;
    v_world_position = world_position.xyz;
    v_world_normal = normalize(normal_matrix * vertex.normal.xyz);
    v_world_tangent = vec4(normalize(normal_matrix * vertex.tangent.xyz), vertex.tangent.w);
    v_material_id = instance.materialId;

    gl_Position = viewProjection * world_position;
}
