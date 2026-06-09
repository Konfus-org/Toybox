layout(location = 0) out vec2 v_tex_coord;
layout(location = 1) out vec4 v_color;
layout(location = 3) out vec3 v_world_position;
layout(location = 4) out vec3 v_world_normal;
layout(location = 5) out vec4 v_world_tangent;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec2 a_uv;
layout(location = 4) in vec4 a_tangent;

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
    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix)));
    v_world_position = worldPosition.xyz;
    v_world_normal = normalMatrix * a_normal;
    v_world_tangent = vec4(normalize(normalMatrix * a_tangent.xyz), a_tangent.w);
    gl_Position = viewProjection * worldPosition;
}
