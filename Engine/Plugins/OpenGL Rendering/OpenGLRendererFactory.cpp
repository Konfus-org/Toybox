#include "OpenGLRendererFactory.h"
#include "OpenGLRenderer.h"

namespace OpenGLRendering
{
    Tbx::IRenderer* OpenGLRendererFactory::Create()
    {
        return new OpenGLRenderer();
    }

    void OpenGLRendererFactory::Destroy(Tbx::IRenderer* rendererToDestroy)
    {
        delete rendererToDestroy;
    }

    void OpenGLRendererFactory::OnLoad()
    {
    }

    void OpenGLRendererFactory::OnUnload()
    {
    }
}

TBX_REGISTER_PLUGIN(OpenGLRendering::OpenGLRendererFactory);