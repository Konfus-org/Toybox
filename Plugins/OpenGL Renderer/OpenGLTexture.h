#pragma once
#include "IBindable.h"
#include <TbxCore.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace OpenGLRendering
{
    class OpenGLTexture : public IBindable
    {
    public:
        OpenGLTexture();
        ~OpenGLTexture() final;

        void SetData(const Tbx::Texture& tex) const;
        void Bind() const final;
        void Unbind() const final;

    private:
        Tbx::uint32 _rendererId = -1;
    };
}

