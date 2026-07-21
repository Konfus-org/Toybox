#include "ShaderBase.glsl"

// Depth-only caster pass. SSBO-pull like Pbr.vert, but transforms into the active cascade's light
// clip space (shadowPassViewProjection, set per cascade) instead of the camera's, so the rendered
// depth buffer is the scene as the sun sees it. One pipeline draws every caster per cascade;
// material/lighting outputs are irrelevant. The fragment stage screen-door dithers the caster by its
// per-instance shadow fade, so a shrinking caster's shadow thins out smoothly instead of popping.
layout(location = 0) out flat float v_shadow_fade;

void main()
{
    InstanceData instance = instances[gl_BaseInstance];
    VertexData vertex = vertices[gl_VertexID];
    v_shadow_fade = instance.shadowFade;
    gl_Position = shadowPassViewProjection * (instance.modelMatrix * vec4(vertex.position.xyz, 1.0));
}
