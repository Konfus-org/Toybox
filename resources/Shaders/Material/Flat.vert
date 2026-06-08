#include "ShaderBase.glsl"

layout(location = 0) out vec2 v_tex_coord;
layout(location = 1) out vec4 v_color;
layout(location = 2) flat out uint v_material_id;

void main()
{
    uint instance_id = gl_BaseInstance + uint(gl_InstanceID);
    InstanceData instance = instances[instance_id];
    VertexData vertex = vertices[uint(gl_VertexID)];

    v_tex_coord = vertex.uv.xy;
    v_color = vertex.color;
    v_material_id = instance.materialId;

    vec4 worldPosition = instance.modelMatrix * vec4(vertex.position.xyz, 1.0);
    gl_Position = viewProjection * worldPosition;
}
