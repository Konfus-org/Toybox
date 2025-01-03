#pragma once
#include <TbxCore.h>
#include <glad/glad.h>

namespace OpenGLRendering
{
    class OpenGLShader : public Tbx::IShader
    {
    public:
        ~OpenGLShader() final;

        void Compile(const std::string& vertexShader, const std::string& fragmentShader) override;
        void Bind() const override;
        void Unbind() const override;

    private:
        Tbx::uint _rendererId = -1;
    };

    static GLenum ShaderDataTypeToOpenGLType(const Tbx::ShaderDataType& type)
    {
        using enum Tbx::ShaderDataType;
        switch (type)
        {
        case None:     return GL_NONE;
        case Float:    return GL_FLOAT;
        case Float2:   return GL_FLOAT;
        case Float3:   return GL_FLOAT;
        case Float4:   return GL_FLOAT;
        case Mat3:     return GL_FLOAT;
        case Mat4:     return GL_FLOAT;
        case Int:      return GL_INT;
        case Int2:     return GL_INT;
        case Int3:     return GL_INT;
        case Int4:     return GL_INT;
        case Bool:     return GL_BOOL;
        }

        TBX_ASSERT(false, "Couln not convert to OpenGL type from ShaderDataType, given unknown ShaderDataType!");
        return GL_NONE;
    }
}
