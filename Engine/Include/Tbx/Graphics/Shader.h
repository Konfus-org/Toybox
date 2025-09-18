#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/TypeAliases/Int.h"
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

    EXPORT inline uint32 GetShaderDataTypeSize(ShaderUniformDataType type)
    {
        switch (type)
        {
            using enum ShaderUniformDataType;
            case None:   return 0;
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
        std::string Name = "";
        std::any Data = nullptr;
        ShaderUniformDataType DataType = ShaderUniformDataType::None;
    };

    /// <summary>
    /// The type of a shader.
    /// </summary>
    enum class EXPORT ShaderType
    {
        None,
        Vertex,
        Fragment,
        Compute // NOT YET SUPPORTED!
    };

    /// <summary>
    /// An GLSL shader.
    /// </summary>
    struct Shader
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
}