#version 330 core

layout(location = 0) out vec4 OutColor;

in vec4 Color;
in vec4 VertColor;
in vec3 Normal; // TODO: implement normals!
in vec2 TextureCoord;

uniform sampler2D TextureUniform;

void main()
{
    vec4 textureColor = Color;
    textureColor *= texture(TextureUniform, TextureCoord);
    OutColor = textureColor;
}