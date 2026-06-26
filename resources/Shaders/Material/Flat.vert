#include "ShaderBase.glsl"

// Forward+ unlit vertex. SSBO-pull like Pbr.vert but only flows uv/color/material id.
layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_color;
layout(location = 5) out flat uint v_material_id;
layout(location = 6) out flat float v_fade; // size/LOD screen-door fade (1 = opaque)

void main()
{
    InstanceData instance = instances[gl_BaseInstance];
    VertexData vertex = vertices[gl_VertexID];

    v_uv = vertex.uv.xy;
    v_color = vertex.color;
    v_material_id = instance.materialId;
    v_fade = instance.renderFade;

    gl_Position = viewProjection * (instance.modelMatrix * vec4(vertex.position.xyz, 1.0));
}
