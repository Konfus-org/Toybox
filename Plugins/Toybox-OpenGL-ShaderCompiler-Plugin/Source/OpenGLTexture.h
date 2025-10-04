#pragma once
#include "IBindable.h"
#include <Tbx/Graphics/Texture.h>

namespace OpenGLRendering
{
    class OpenGLTexture final : public IBindable
    {
    public:
        OpenGLTexture();
        ~OpenGLTexture() override;

        void Bind() const override;
        void Unbind() const override;

        void Upload(const Tbx::Texture& tex, const Tbx::uint& slot);

    private:
        Tbx::uint _slot = 0;
        Tbx::uint32 _textureGLId = -1;
    };
}

