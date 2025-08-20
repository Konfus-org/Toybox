#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TypeAliases/Int.h"
#include "Tbx/Ids/UsesUID.h"
#include <any>

namespace Tbx
{
    enum class EXPORT ShaderUniformDataType
    {
        None = 0,
        Float,
        Float2,
        Float3,
        Float4,
        Mat3,
        Mat4,
        Int,
        Int2,
        Int3,
        Int4,
        Bool
    };

    static uint32 GetShaderDataTypeSize(ShaderUniformDataType type)
    {
        switch (type)
        {
            using enum ShaderUniformDataType;
            case Float:  return 4;
            case Float2: return 4 * 2;
            case Float3: return 4 * 3;
            case Float4: return 4 * 4;
            case Mat3:   return 4 * 3 * 3;
            case Mat4:   return 4 * 4 * 4;
            case Int:    return 4;
            case Int2:   return 4 * 2;
            case Int3:   return 4 * 3;
            case Int4:   return 4 * 4;
            case Bool:   return 1;
        }

        return 0;
    }

    struct ShaderUniform 
    {
        const std::string Name;
        const std::any Data;
        const ShaderUniformDataType DataType;
    };

    /// <summary>
    /// A vertex shader uniform.
    /// </summary>
    struct VertexUniform : ShaderUniform {};

    /// <summary>
    /// A fragment shader uniform.
    /// </summary>
    struct FragmentUniform : ShaderUniform {};

    /// <summary>
    /// The type of a shader.
    /// </summary>
    enum class EXPORT ShaderType
    {
        Vertex,
        Fragment,
        Compute // NOT YET SUPPORTED!
    };

    /// <summary>
    /// An GLSL shader.
    /// </summary>
    class Shader : public UsesUid
    {
    public:
        /// <summary>
        /// Will create the default Tbx shader.
        /// </summary>
        EXPORT Shader() = default;

        /// <summary>
        /// Will create a custom shader with the given source code.
        /// </summary>
        EXPORT Shader(const std::string& src, ShaderType type)
            : _src(src), _type(type) {}

        EXPORT const std::string& GetSource() const { return _src; }
        EXPORT ShaderType GetType() const { return _type; }

    private:
        std::string _src = "";
        ShaderType _type = ShaderType::Vertex;
    };

    /// <summary>
    /// The default Tbx vertex shader in GLSL.
    /// </summary>
    EXPORT inline const Shader& DefaultFragmentShader
    {
        R"(
            #version 330 core

            layout(location = 0) out vec4 OutColor;

            in vec4 Color;
            in vec4 VertColor;
            in vec4 Normal; // TODO: implement normals!
            in vec2 TextureCoord;

            uniform sampler2D TextureUniform;

            void main()
            {
                vec4 textureColor = Color;
                textureColor *= texture(TextureUniform, InTextureCoord);
                OutColor = textureColor;
            }
        )",
        ShaderType::Fragment
    };

    /// <summary>
    /// The default Tbx fragment shader in GLSL.
    /// </summary>
    EXPORT inline const Shader& DefaultVertexShader
    {
        R"(
            #version 330 core

            layout(location = 0) in vec3 InPosition;
            layout(location = 1) in vec4 InVertColor;
            layout(location = 2) in vec4 InNormal; // TODO: implement normals!
            layout(location = 3) in vec2 InTextureCoord;

            out vec4 Color;
            out vec4 VertColor;
            out vec4 Normal;
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
        )",
        ShaderType::Vertex
    };
}