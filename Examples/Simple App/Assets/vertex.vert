#version 330 core

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec4 InVertColor;
layout(location = 2) in vec3 InNormal; // TODO: implement normals!
layout(location = 3) in vec2 InTextureCoord;

out vec4 Color;
out vec4 VertColor;
out vec2 TextureCoord;

uniform mat4 ViewProjectionUniform;
uniform mat4 TransformUniform;
uniform vec4 ColorUniform;

void main()
{
    Color = ColorUniform;
    VertColor = InVertColor;
    TextureCoord = InTextureCoord;
    gl_Position = ViewProjectionUniform * TransformUniform * vec4(InPosition, 1.0);
}