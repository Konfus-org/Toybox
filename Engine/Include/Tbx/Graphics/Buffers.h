#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/DrawCommand.h"
#include <string>
#include <vector>

namespace Tbx
{
    struct BufferElement
    {
    public:
        EXPORT BufferElement() = default;
        EXPORT BufferElement(ShaderUniformDataType type, const std::string& name, bool normalized = false)
            : _name(name), _size(GetShaderDataTypeSize(type)), _type(type), _normalized(normalized) {}
        EXPORT BufferElement(ShaderUniformDataType type, const std::string& name, uint32 offset, bool normalized = false)
            : _name(name), _size(GetShaderDataTypeSize(type)), _offset(offset), _type(type), _normalized(normalized) {}

        EXPORT bool IsNormalized() const { return _normalized; }
        EXPORT std::string GetName() const { return _name; }
        EXPORT uint32 GetSize() const { return _size; }
        EXPORT ShaderUniformDataType GetType() const { return _type; }

        EXPORT uint32 GetCount() const 
        {
            switch (_type)
            {
                using enum ShaderUniformDataType;
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
            
            return 0;
        }

        EXPORT uint32 GetOffset() const { return _offset; }
        EXPORT void SetOffset(const uint32& offset) { _offset = offset; }

    private:
        std::string _name = "";
        uint32 _size = 0;
        uint32 _offset = 0;
        ShaderUniformDataType _type = ShaderUniformDataType::None;
        bool _normalized = false;
    };

    class BufferLayout
    {
    public:
        EXPORT BufferLayout() = default;
        explicit(false) EXPORT BufferLayout(const std::vector<BufferElement>& elements) : _elements(elements)
        {
            CalculatOffsetsAndStride();
        }
        explicit(false) EXPORT BufferLayout(const std::initializer_list<BufferElement>& elements) : _elements(elements)
        {
            CalculatOffsetsAndStride();
        }

        EXPORT const std::vector<BufferElement>& GetElements() const { return _elements; }
        EXPORT uint32 GetStride() const { return _stride; }

        std::vector<BufferElement>::iterator begin() { return _elements.begin(); }
        std::vector<BufferElement>::iterator end() { return _elements.end(); }
        std::vector<BufferElement>::const_iterator begin() const { return _elements.begin(); }
        std::vector<BufferElement>::const_iterator end() const { return _elements.end(); }

    private:
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

        std::vector<BufferElement> _elements;
        uint32 _stride = 0;
    };

    struct VertexBuffer
    {
    public:
        EXPORT VertexBuffer() = default;
        EXPORT explicit(false) VertexBuffer(const std::vector<float>& vertices, const BufferLayout& layout) 
            : _vertices(vertices), _layout(layout) {}
        EXPORT explicit(false) VertexBuffer(const std::initializer_list<float>& vertices, const BufferLayout& layout)
            : _vertices(vertices), _layout(layout) {}

        EXPORT const BufferLayout& GetLayout() const { return _layout; }
        EXPORT std::vector<float> GetVertices() const { return _vertices; }

    private:
        std::vector<float> _vertices;
        BufferLayout _layout;
    };

    /// <summary>
    /// Represents one frame worth of draw commands
    /// </summary>
    struct FrameBuffer
    {
    public:
        EXPORT FrameBuffer() = default;
        EXPORT explicit(false) FrameBuffer(const std::vector<DrawCommand>& commands) : _commands(commands) {}

        EXPORT void Emplace(const DrawCommandType& type, const std::any& payload) { _commands.emplace_back(type, payload); }
        EXPORT void Add(const DrawCommand& command) { _commands.push_back(command); }
        EXPORT void Clear() { _commands.clear(); }

        const std::vector<DrawCommand>& GetCommands() const { return _commands; }

        EXPORT std::vector<DrawCommand>::iterator begin() { return _commands.begin(); }
        EXPORT std::vector<DrawCommand>::iterator end() { return _commands.end(); }
        EXPORT std::vector<DrawCommand>::const_iterator begin() const { return _commands.begin(); }
        EXPORT std::vector<DrawCommand>::const_iterator end() const { return _commands.end(); }

    private:
        std::vector<DrawCommand> _commands;
    };
}
