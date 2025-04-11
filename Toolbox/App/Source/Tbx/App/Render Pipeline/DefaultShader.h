#pragma once
#include "Tbx/Core/Rendering/Shader.h"

namespace Tbx
{
    const auto& DefaultShaderVertexSrc = R"(
            #version 330 core

            layout(location = 0) in vec3 inPosition;
            layout(location = 1) in vec4 inColor;
            layout(location = 2) in vec4 inNormal; // TODO: implement normals!
            layout(location = 3) in vec2 inTextureCoord;

            uniform mat4 viewProjection;
            uniform mat4 transform;

            out vec4 color;
            out vec4 normal;
            out vec2 textureCoord;
            
            void main()
            {
                color = inColor;
                textureCoord = inTextureCoord;
                gl_Position = viewProjection * transform * vec4(inPosition, 1.0);
            }
        )";

    const auto& DefaultShaderFragmentSrc = R"(
            #version 330 core

            layout(location = 0) out vec4 outColor;

            in vec4 color;
            in vec4 normal; // TODO: implement normals!
            in vec2 textureCoord;

            uniform sampler2D textureUniform;
            
            void main()
            {
                outColor = texture(textureUniform, textureCoord) * color;
            }
        )";

    const Shader& DefaultShader = Shader(DefaultShaderVertexSrc, DefaultShaderFragmentSrc);
}