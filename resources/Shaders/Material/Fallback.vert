#include "ShaderBase.glsl"

// "Magenta Unlit" vertex. SSBO-pull like the material shaders, flowing only uv + material id so the
// fragment can sample the fallback material's magenta checkerboard. Depends solely on the scene
// transform + geometry SSBOs, so it renders even when other material data is broken.
layout(location = 0) out vec2 v_uv;
layout(location = 5) out flat uint v_material_id;

void main()
{
    InstanceData instance = instances[gl_BaseInstance];
    VertexData vertex = vertices[gl_VertexID];
    v_uv = vertex.uv.xy;
    v_material_id = instance.materialId;
    gl_Position = viewProjection * (instance.modelMatrix * vec4(vertex.position.xyz, 1.0));
}
