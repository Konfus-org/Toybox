#include "ShaderBase.glsl"

// Depth-only caster pass. SSBO-pull like Pbr.vert, but transforms into the directional light's clip
// space (lightViewProjection) instead of the camera's, so the rendered depth buffer is the scene as
// the sun sees it. One pipeline draws every shadow caster; material/lighting outputs are irrelevant.
void main()
{
    InstanceData instance = instances[gl_BaseInstance];
    VertexData vertex = vertices[gl_VertexID];
    gl_Position = lightViewProjection * (instance.modelMatrix * vec4(vertex.position.xyz, 1.0));
}
