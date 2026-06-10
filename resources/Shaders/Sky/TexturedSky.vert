#include "ShaderBase.glsl"

// Forward+ sky vertex. Like every pipeline shader it pulls its inputs from the global SSBOs (no
// vertex attributes are bound). The sky mesh is centered on the camera (CPU builds the model matrix
// with the camera position as its translation), so the local direction toward each vertex doubles
// as the sample direction. gl_Position is emitted with z = w so the sky always lands exactly on the
// far plane and is never clipped, regardless of the mesh's scale or the camera's near/far range.
layout(location = 0) out vec3 v_sky_direction;
layout(location = 1) out flat uint v_material_id;

void main()
{
    InstanceData instance = instances[gl_BaseInstance];
    VertexData vertex = vertices[gl_VertexID];

    vec4 world_position = instance.modelMatrix * vec4(vertex.position.xyz, 1.0);
    v_sky_direction = world_position.xyz - cameraPositionTime.xyz;
    v_material_id = instance.materialId;

    vec4 clip = viewProjection * world_position;
    gl_Position = clip.xyww;
}
