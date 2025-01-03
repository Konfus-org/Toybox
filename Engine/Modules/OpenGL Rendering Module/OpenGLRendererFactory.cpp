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

    std::string OpenGLRendererFactory::GetName() const
    {
        return "OpenGL Rendering";
    }

    std::string OpenGLRendererFactory::GetAuthor() const
    {
        return "Jeremy Hummel";
    }

    int OpenGLRendererFactory::GetVersion() const
    {
        return 0;
    }
}

Tbx::Module* Load()
{
    return new OpenGLRendering::OpenGLRendererFactory();
}

void Unload(Tbx::Module* moduleToUnload)
{
    delete moduleToUnload;
}