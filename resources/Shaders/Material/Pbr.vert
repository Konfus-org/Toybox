#include "Base/MaterialShaderBase.glsl"

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec2 a_tex_coord;
layout(location = 4) in vec4 a_tangent;

layout(location = 0) out vec2 v_tex_coord;
layout(location = 1) out vec4 v_color;
layout(location = 2) flat out uint v_material_id;

void main()
{
    uint visibleIndex = gl_BaseInstance + uint(gl_InstanceID);
    EntityData entity = tbx_load_visible_entity(visibleIndex);

    v_tex_coord = a_tex_coord;
    v_color = a_color;
    v_material_id = entity.materialId;

    vec4 worldPosition = entity.modelMatrix * vec4(a_position, 1.0);
    gl_Position = viewProjection * worldPosition;
}
