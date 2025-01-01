#pragma once
#include "tbxpch.h"
#include "tbxAPI.h"
#include "Vertex.h"
#include "Shader.h"
#include "Math/Math.h"

namespace Toybox
{
    struct BufferElement
    {
    public:
        TBX_API BufferElement() = default;

        TBX_API BufferElement(const ShaderDataType& type, const std::string& name, const bool& normalized = false)
            : _name(name), _size(GetShaderDataTypeSize(type)), _type(type), _normalized(normalized) {}

        TBX_API BufferElement(const ShaderDataType& type, const std::string& name, const uint32& offset, const bool& normalized = false)
            : _name(name), _size(GetShaderDataTypeSize(type)), _offset(offset), _type(type), _normalized(normalized) {}

        TBX_API bool IsNormalized() const { return _normalized; }
        TBX_API const std::string& GetName() const { return _name; }
        TBX_API const uint32& GetSize() const { return _size; }
        TBX_API const ShaderDataType& GetType() const { return _type; }
        TBX_API uint32 GetCount() const 
        {
            using enum Toybox::ShaderDataType;
            switch (_type)
            {
                case None:   return 0;
                case Float:  return 1;
                case Float2: return 2;
                case Float3: return 3;
                case Float4: return 4;
                case Mat3:   return 9;
                case Mat4:   return 16;
                case Int:    return 1;
                case Int2:   return 2;
                case Int3:   return 3;
                case Int4:   return 4;
                case Bool:   return 1;
            }
            
            TBX_ASSERT(false, "Buffer element has unknown ShaderDataType!");
            return 0;
        }

        TBX_API const uint32& GetOffset() const { return _offset; }
        TBX_API void SetOffset(const uint32& offset) { _offset = offset; }

    private:
        std::string _name = "";
        uint32 _size = 0;
        uint32 _offset = 0;
        ShaderDataType _type = ShaderDataType::None;
        bool _normalized = false;
    };

    class BufferLayout
    {
    public:
        TBX_API BufferLayout() = default;

        explicit(false) TBX_API BufferLayout(const std::initializer_list<BufferElement>& elements) : _elements(elements)
        {
            CalculatOffsetsAndStride();
        }

        explicit(false) TBX_API BufferLayout(const std::vector<BufferElement>& elements) : _elements(elements)
        {
            CalculatOffsetsAndStride();
        }

        TBX_API const std::vector<BufferElement>& GetElements() const { return _elements; }
        TBX_API const uint32& GetStride() const { return _stride; }

        TBX_API std::vector<BufferElement>::iterator begin() { return _elements.begin(); }
        TBX_API std::vector<BufferElement>::iterator end() { return _elements.end(); }
        TBX_API std::vector<BufferElement>::reverse_iterator rbegin() { return _elements.rbegin(); }
        TBX_API std::vector<BufferElement>::reverse_iterator rend() { return _elements.rend(); }

        TBX_API std::vector<BufferElement>::const_iterator begin() const { return _elements.begin(); }
        TBX_API std::vector<BufferElement>::const_iterator end() const { return _elements.end(); }
        TBX_API std::vector<BufferElement>::const_reverse_iterator rbegin() const { return _elements.rbegin(); }
        TBX_API std::vector<BufferElement>::const_reverse_iterator rend() const { return _elements.rend(); }

    private:
        std::vector<BufferElement> _elements;
        uint32 _stride = 0;

        void CalculatOffsetsAndStride()
        {
            _stride = 0;
            uint32 offset = 0;
            for (auto& element : _elements)
            {
                element.SetOffset(offset);
                offset += element.GetSize();
                _stride += element.GetSize();
            }
        }
    };

    struct VertexBuffer
    {
    public:
        TBX_API explicit(false) VertexBuffer(const std::vector<Vertex>& vertices) : _vertices(FlattenVertexVector(vertices)) {}
        TBX_API explicit(false) VertexBuffer(const std::vector<float>& vertices) : _vertices(vertices) {}

        TBX_API std::vector<float> GetVertices() const { return _vertices; }

    private:
        std::vector<float> _vertices;
    };

    struct IndexBuffer
    {
    public:
        TBX_API explicit(false) IndexBuffer(const std::vector<uint>& indices) : _indices(indices) {}
        TBX_API std::vector<uint32> GetIndices() const { return _indices; }

    private:
        std::vector<uint32> _indices;
    };
}
