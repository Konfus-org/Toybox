#include "ShaderBase.glsl"

// Forward+ unlit vertex. SSBO-pull like Pbr.vert but only flows uv/color/material id.
layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;
layout(location = 5) out flat uint v_material_id;

void main()
{
    InstanceData instance = instances[gl_BaseInstance];
    VertexData vertex = vertices[gl_VertexID];

    v_uv = vertex.uv.xy;
    v_color = vertex.color;
    v_material_id = instance.materialId;

    gl_Position = viewProjection * (instance.modelMatrix * vec4(vertex.position.xyz, 1.0));
}
