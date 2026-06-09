layout(location = 0) out vec2 v_tex_coord;
layout(location = 1) out vec4 v_color;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 3) in vec2 a_uv;

layout(std140, binding = 0) uniform TbxFrame
{
    mat4 viewProjection;
    vec4 ambientLight;
};

layout(std140, binding = 1) uniform TbxObject
{
    mat4 modelMatrix;
};

void main()
{
    v_tex_coord = a_uv;
    v_color = a_color;
    vec4 worldPosition = modelMatrix * vec4(a_position, 1.0);
    gl_Position = viewProjection * worldPosition;
}
