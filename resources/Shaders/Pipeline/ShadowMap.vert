#include "ShaderBase.glsl"

void main()
{
    uint packed_instance = gl_BaseInstance + uint(gl_InstanceID);
    uint divisor = max(totalInstanceCount, 1u);
    uint light_id = min(packed_instance / divisor, max(lightCount, 1u) - 1u);
    uint instance_id = packed_instance % divisor;
    InstanceData instance = instances[instance_id];
    LightData light = lights[light_id];
    VertexData vertex = vertices[uint(gl_VertexID)];
    vec4 world_position = instance.modelMatrix * vec4(vertex.position.xyz, 1.0);

    gl_Position = tbx_make_light_view_projection(light) * world_position;
    gl_Layer = int(light.shadowData.x);
}
