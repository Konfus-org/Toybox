#pragma once
#include "TbxAPI.h"
#include "TbxPCH.h"
#include "Math/Int.h"
#include "Math/Matrix.h"
#include "Debug/DebugAPI.h"

namespace Tbx
{
    enum class TBX_API ShaderDataType
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
        using enum Tbx::ShaderDataType;
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
            default:     return 0;
        }

        TBX_ASSERT(false, "Unknown ShaderDataType!");
        return 0;
    }

    struct ShaderData
    {
    public:
        TBX_API ShaderData(const std::string& name, const Matrix& data) : _name(name), _data(data) {}
        TBX_API const std::string& GetName() const { return _name; }
        TBX_API const Matrix& GetData() const { return _data; }

    private:
        std::string _name;
        Matrix _data;
    };

    class  Shader
    {
    public:
        TBX_API Shader(const std::string_view& vertexSrc, const std::string_view& fragmentSrc) : _vertexSrc(vertexSrc), _fragmentSrc(fragmentSrc) {}
        TBX_API ~Shader() = default;

        TBX_API const std::string& GetVertexSource() const { return _vertexSrc; }
        TBX_API const std::string& GetFragmentSource() const { return _fragmentSrc; }

    private:
        std::string _vertexSrc;
        std::string _fragmentSrc;
    };
}