#pragma once
#include "tbxpch.h"
#include "tbxAPI.h"
#include "Vertex.h"
#include "Shader.h"
#include "Math/Math.h"

namespace Toybox
{
    struct VertexBuffer
    {
    public:
        TBX_API explicit(false) VertexBuffer(const std::vector<Vertex>& vertices)
        {
            auto numberOfVertices = vertices.size();
            std::vector<float> meshPoints(numberOfVertices * 3);
            int positionToPlace = 0;
            for (const auto& vertex : vertices)
            {
                const auto& position = vertex.Position;
                meshPoints[positionToPlace] = position.X;
                meshPoints[positionToPlace + 1] = position.Y;
                meshPoints[positionToPlace + 2] = position.Z;
                positionToPlace += 3;
            }
            _vertices = meshPoints;
        }

        TBX_API explicit(false) VertexBuffer(const std::vector<float>& vertices) : _vertices(vertices) {}
        TBX_API inline std::vector<float> GetVertices() const { return _vertices; }

    private:
        std::vector<float> _vertices;
    };

    struct IndexBuffer
    {
    public:
        TBX_API explicit(false) IndexBuffer(const std::vector<uint>& indices) : _indices(indices) {}
        TBX_API inline std::vector<uint32> GetIndices() const { return _indices; }

    private:
        std::vector<uint32> _indices;
    };

    struct BufferElement
    {
    public:
        TBX_API BufferElement(const ShaderDataType& type, const std::string& name)
            : _name(name), _size(GetShaderDataTypeSize(type)), _offset(0), _type(type) {}
        TBX_API BufferElement(const ShaderDataType& type, const std::string& name, const uint32& offset)
            : _name(name), _size(GetShaderDataTypeSize(type)), _offset(offset), _type(type) {}

        TBX_API inline const std::string& GetName() const { return _name; }
        TBX_API inline const uint32& GetSize() const { return _size; }
        TBX_API inline const ShaderDataType& GetType() const { return _type; }

        TBX_API inline const uint32& GetOffset() const { return _offset; }
        TBX_API inline void SetOffset(const uint32& offset) { _offset = offset; }

    private:
        std::string _name;
        uint32 _size;
        uint32 _offset;
        ShaderDataType _type;
    };

    class BufferLayout
    {
    public:
        TBX_API BufferLayout(const std::initializer_list<BufferElement>& elements) : _elements(elements)
        {
            CalculatOffsetsAndStride();
        }

        explicit(false) TBX_API BufferLayout(const std::vector<BufferElement>& elements) : _elements(elements)
        {
            CalculatOffsetsAndStride();
        }

        inline const TBX_API std::vector<BufferElement>& GetElements() const { return _elements; }

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
}
