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

        void SetData(const Tbx::Texture& tex, const Tbx::uint& slot);
        void Bind() const final;
        void Unbind() const final;

        Tbx::UID GetAssociatedAssetId() const { return _associatedAssetId; }

    private:
        Tbx::uint _slot = 0;
        Tbx::UID _associatedAssetId = -1;
        Tbx::uint32 _rendererId = -1;
    };
}

