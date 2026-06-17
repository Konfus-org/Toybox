#include "ShaderBase.glsl"

// Translucent (colored) caster pass. Like ShadowDepth.vert it transforms into the active cascade's
// light clip space (shadowPassViewProjection — here the full-range/furthest cascade), but it also
// forwards the material id so the fragment stage can read the caster's transparent tint and write it
// into the transmittance shadow map.
layout(location = 0) out flat uint v_material_id;

void main()
{
    InstanceData instance = instances[gl_BaseInstance];
    VertexData vertex = vertices[gl_VertexID];
    v_material_id = instance.materialId;
    gl_Position = shadowPassViewProjection * (instance.modelMatrix * vec4(vertex.position.xyz, 1.0));
}
