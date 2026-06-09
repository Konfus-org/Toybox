layout(location = 0) in vec2 v_tex_coord;
layout(location = 1) in vec4 v_color;
layout(location = 3) in vec3 v_world_position;
layout(location = 4) in vec3 v_world_normal;
layout(location = 5) in vec4 v_world_tangent;

layout(location = 0) out vec4 o_color;

layout(std140, binding = 0) uniform TbxFrame
{
    mat4 viewProjection;
    vec4 ambientLight;
};

layout(std140, binding = 2) uniform TbxAlbedoColor
{
    vec4 albedoColor;
};

layout(std140, binding = 3) uniform TbxEmissiveColor
{
    vec4 emissiveColor;
};

layout(std140, binding = 4) uniform TbxMetallic
{
    float metallic;
};

layout(std140, binding = 5) uniform TbxRoughness
{
    float roughness;
};

layout(std140, binding = 6) uniform TbxNormalStrength
{
    float normalStrength;
};

layout(std140, binding = 7) uniform TbxAo
{
    float ao;
};

layout(binding = 16) uniform sampler2D albedoMap;
layout(binding = 17) uniform sampler2D normalMap;
layout(binding = 18) uniform sampler2D metallicMap;
layout(binding = 19) uniform sampler2D roughnessMap;
layout(binding = 20) uniform sampler2D aoMap;
layout(binding = 21) uniform sampler2D emissiveMap;

void main()
{
    vec3 normal = normalize(v_world_normal);
    vec3 tangent = normalize(v_world_tangent.xyz);
    vec3 bitangent = normalize(cross(normal, tangent) * v_world_tangent.w);
    vec3 sampledNormal = texture(normalMap, v_tex_coord).xyz * 2.0 - 1.0;
    sampledNormal.xy *= normalStrength;
    normal = normalize(mat3(tangent, bitangent, normal) * sampledNormal);

    vec3 lightDirection = normalize(vec3(0.35, 0.75, 0.45));
    vec3 viewDirection = normalize(vec3(0.0, 0.0, 1.0));
    vec3 halfDirection = normalize(lightDirection + viewDirection);
    float resolvedMetallic = clamp(metallic * texture(metallicMap, v_tex_coord).r, 0.0, 1.0);
    float resolvedRoughness = clamp(roughness * texture(roughnessMap, v_tex_coord).r, 0.04, 1.0);
    float resolvedAo = clamp(ao * texture(aoMap, v_tex_coord).r, 0.0, 1.0);
    float light = max(dot(normal, lightDirection), 0.0);
    float specularPower = mix(96.0, 8.0, resolvedRoughness);
    float specularLight = pow(max(dot(normal, halfDirection), 0.0), specularPower);

    vec4 surface = v_color * albedoColor * texture(albedoMap, v_tex_coord);
    vec3 diffuse = surface.rgb * (ambientLight.rgb + vec3(light * 0.85 * resolvedAo));
    vec3 specularColor = mix(vec3(0.04), surface.rgb, resolvedMetallic);
    vec3 emissive = emissiveColor.rgb * texture(emissiveMap, v_tex_coord).rgb;
    o_color = vec4(diffuse + specularColor * specularLight + emissive, surface.a);
}
