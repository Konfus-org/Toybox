#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Ids/UsesUID.h"
#include "Tbx/Core/Debug/DebugAPI.h"
#include <Tbx/Math/Int.h>
#include <any>

namespace Tbx
{
    enum class EXPORT ShaderDataType
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

    static uint32 GetShaderDataTypeSize(ShaderDataType type)
    {
        using enum ShaderDataType;
        switch (type)
        {
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

        TBX_ASSERT(false, "Unknown ShaderDataType!");
        return 0;
    }

    struct ShaderData
    {
    public:
        EXPORT ShaderData(const std::string& name, const std::any& data, const ShaderDataType& type) 
            : _name(name), _data(data), _type(type) {}

        EXPORT std::string GetName() const { return _name; }
        EXPORT const std::any& GetData() const { return _data; }
        EXPORT ShaderDataType GetType() const { return _type; }

    private:
        std::string _name;
        std::any _data;
        ShaderDataType _type;
    };

    class Shader : public UsesUID
    {
    public:
        /// <summary>
        /// Will create the default Tbx shader.
        /// </summary>
        EXPORT Shader() = default;

        /// <summary>
        /// Will create a custom shader with the given vertex and fragment source.
        /// </summary>
        EXPORT Shader(const std::string_view& vertexSrc, const std::string_view& fragmentSrc)
            : _vertexSrc(vertexSrc), _fragmentSrc(fragmentSrc) {}

        EXPORT const std::string& GetVertexSource() const { return _vertexSrc; }
        EXPORT const std::string& GetFragmentSource() const { return _fragmentSrc; }

    private:
        std::string _vertexSrc = "";
        std::string _fragmentSrc = "";
    };

    namespace Shaders
    {
        EXPORT inline const Shader& DefaultShader
        {
            R"(
                #version 330 core

                layout(location = 0) in vec3 inPosition;
                layout(location = 1) in vec4 inVertColor;
                layout(location = 2) in vec4 inNormal; // TODO: implement normals!
                layout(location = 3) in vec2 inTextureCoord;

                uniform mat4 viewProjectionUni;
                uniform mat4 transformUni;
                uniform vec4 colorUni;

                out vec4 color;
                out vec4 vertColor;
                out vec4 normal;
                out vec2 textureCoord;
                
                void main()
                {
                    color = colorUni;
                    vertColor = inVertColor;
                    textureCoord = inTextureCoord;
                    gl_Position = viewProjectionUni * transformUni * vec4(inPosition, 1.0);
                }
            )",
        R"(
                #version 330 core

                layout(location = 0) out vec4 outColor;

                in vec4 color;
                in vec4 vertColor;
                in vec4 normal; // TODO: implement normals!
                in vec2 textureCoord;

                uniform sampler2D textureUniform;
                
                void main()
                {
                    vec4 texColor = color;
                    texColor *= texture(textureUniform, textureCoord);
                    outColor = texColor;
                }
            )"
        };
    }
}