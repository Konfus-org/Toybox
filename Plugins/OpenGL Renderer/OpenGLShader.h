#pragma once
#include "IBindable.h"
#include <TbxCore.h>
#include <glad/glad.h>

namespace OpenGLRendering
{
    class OpenGLShader : public IBindable
    {
    public:
        ~OpenGLShader() final;

        void Compile(const Tbx::Shader& shader);
        void Bind() const final;
        void Unbind() const final;
        void UploadData(const Tbx::ShaderData& data) const;

        Tbx::UID GetAssociatedAssetId() const { return _associatedAssetId; }

    private:
        Tbx::UID _associatedAssetId = -1;
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

