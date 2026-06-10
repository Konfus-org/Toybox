#include "ShaderBase.glsl"

// Depth-only caster pass. SSBO-pull like Pbr.vert, but transforms into the active cascade's light
// clip space (shadowPassViewProjection, set per cascade) instead of the camera's, so the rendered
// depth buffer is the scene as the sun sees it. One pipeline draws every caster per cascade;
// material/lighting outputs are irrelevant.
void main()
{
    InstanceData instance = instances[gl_BaseInstance];
    VertexData vertex = vertices[gl_VertexID];
    gl_Position = shadowPassViewProjection * (instance.modelMatrix * vec4(vertex.position.xyz, 1.0));
}
