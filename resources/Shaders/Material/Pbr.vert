#include "ShaderBase.glsl"

layout(location = 0) out vec2 v_tex_coord;
layout(location = 1) out vec4 v_color;
layout(location = 2) flat out uint v_material_id;
layout(location = 3) out vec3 v_world_position;
layout(location = 4) out vec3 v_world_normal;
layout(location = 5) out vec4 v_world_tangent;

void main()
{
    uint instance_id = gl_BaseInstance + uint(gl_InstanceID);
    InstanceData instance = instances[instance_id];
    VertexData vertex = vertices[uint(gl_VertexID)];

    v_tex_coord = vertex.uv.xy;
    v_color = vertex.color;
    v_material_id = instance.materialId;

    vec4 worldPosition = instance.modelMatrix * vec4(vertex.position.xyz, 1.0);
    mat3 normalMatrix = transpose(inverse(mat3(instance.modelMatrix)));
    v_world_position = worldPosition.xyz;
    v_world_normal = normalMatrix * vertex.normal.xyz;
    v_world_tangent = vec4(normalize(normalMatrix * vertex.tangent.xyz), vertex.tangent.w);
    gl_Position = viewProjection * worldPosition;
}
