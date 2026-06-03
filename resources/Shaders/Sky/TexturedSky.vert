#include "Base/SceneShaderBase.glsl"

layout(location = 0) in vec3 a_position;
layout(location = 0) out vec3 v_sky_direction;

void main()
{
    v_sky_direction = a_position;
    gl_Position = viewProjection * vec4(a_position, 1.0);
}
