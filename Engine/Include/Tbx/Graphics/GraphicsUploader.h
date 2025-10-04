#pragma once
#include "Tbx/Graphics/GraphicsPipe.h"
#include "Tbx/Graphics/Shader.h"
#include "Tbx/Graphics/Texture.h"
#include "Tbx/Graphics/Mesh.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Ids/Uid.h"

namespace Tbx
{
    /// <summary>
    /// Manages a graphics upload.
    /// Once this object goes out of scope the destructor is responsible for cleaning up the upload.
    /// </summary>
    class TBX_EXPORT GraphicsHandle
    {
    public:
        virtual ~GraphicsHandle() = 0;
        Uid RenderId = Uid::Invalid;
    };

    class TBX_EXPORT IGraphicsUploader : public IGraphicsPipe
    {
    public:
        virtual ~IGraphicsUploader() = 0;
        virtual Ref<GraphicsHandle> UploadShader(Ref<Shader> shader) = 0;
        virtual Ref<GraphicsHandle> UploadTexture(Ref<Texture> texture) = 0;
        virtual Ref<GraphicsHandle> UploadMesh(Ref<Mesh> mesh) = 0;
    };
}