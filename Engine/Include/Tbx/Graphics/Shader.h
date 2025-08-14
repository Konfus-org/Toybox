#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TypeAliases/Int.h"
#include "Tbx/Ids/UsesUID.h"
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

        return 0;
    }

    struct ShaderData
    {
    public:
        EXPORT ShaderData() = default;
        EXPORT ShaderData(const std::string& name, const std::any& data, const ShaderDataType& type) 
            : _name(name), _data(data), _type(type) {}
        EXPORT ShaderData(bool isFragment, uint32 uniformSlot, const void* uniformData, uint32 uniformSize) 
            : _isFragment(isFragment), _uniformSlot(uniformSlot), _uniformData(uniformData), _uniformSize(uniformSize) {}

        EXPORT std::string GetName() const { return _name; }
        EXPORT const std::any& GetPixels() const { return _data; }
        EXPORT ShaderDataType GetType() const { return _type; }

        bool _isFragment;
        uint32 _uniformSlot;
        const void* _uniformData;
        uint32 _uniformSize;

    private:
        std::string _name = "";
        std::any _data = nullptr;
        ShaderDataType _type = ShaderDataType::None;
    };

    enum class EXPORT ShaderType
    {
        Vertex,
        Fragment
    };

    /// <summary>
    /// An HLSL shader.
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

    namespace Shaders
    {
        EXPORT inline const Shader& DefaultVertexShader
        {
            R"(
                // Output
                struct PSOutput
                {
                    float4 outColor : SV_TARGET;
                };

                // Inputs
                struct PSInput
                {
                    float4 color : COLOR0;
                    float4 vertColor : COLOR1;
                    float3 normal : NORMAL;
                    float2 textureCoord : TEXCOORD0;
                };

                // Bindings matching GLSL layout(set=2,binding=0) and (set=3,binding=0)
                Texture2D textureUniform : register(t0, space2);     // set 2, binding 0
                SamplerState textureUniformSampler : register(s0, space2);

                cbuffer UniformBlock : register(b0, space3)          // set 3, binding 0
                {
                    float time;
                };

                PSOutput main(PSInput input)
                {
                    PSOutput output;
                    float4 texColor = textureUniform.Sample(textureUniformSampler, input.textureCoord);
                    output.outColor = texColor;
                    return output;
                }
            )",
            ShaderType::Vertex
        };

        EXPORT inline const Shader& DefaultFragmentShader 
        {
            R"(
                // Inputs
                struct VSInput
                {
                    float3 inPosition    : POSITION;     // location 0
                    float4 inVertColor   : COLOR0;       // location 1
                    float3 inNormal      : NORMAL;       // location 2
                    float2 inTextureCoord: TEXCOORD0;    // location 3
                };

                // Outputs
                struct VSOutput
                {
                    float4 position      : SV_POSITION;  // gl_Position
                    float4 color         : COLOR0;       // location 0
                    float4 vertColor     : COLOR1;       // location 1
                    float3 normal        : NORMAL;       // location 2
                    float2 textureCoord  : TEXCOORD0;    // location 3
                };

                // Uniform buffer at set = 1, binding = 0
                cbuffer UniformBlock : register(b0, space1)
                {
                    float4x4 viewProjectionUni;
                    // float4x4 transformUni;  // Uncomment and add if needed
                    // float4 colorUni;        // Uncomment and add if needed
                };

                VSOutput main(VSInput input)
                {
                    VSOutput output;

                    // Position transform
                    output.position = mul(viewProjectionUni, float4(input.inPosition, 1.0));

                    // Pass through vertex colors
                    output.color = input.inVertColor;

                    // For matching GLSL outputs (some are commented out)
                    output.vertColor = input.inVertColor;  // assuming you want to output this too
                    output.normal = input.inNormal;
                    output.textureCoord = input.inTextureCoord;

                    return output;
                }
            )",
            ShaderType::Fragment
        };
    }
}