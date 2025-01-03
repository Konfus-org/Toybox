#include "OpenGLRenderingModule.h"
#include "OpenGLRenderer.h"

namespace OpenGLRendering
{
    Tbx::IRenderer* OpenGLRenderingModule::Create()
    {
        return new OpenGLRenderer();
    }

    void OpenGLRenderingModule::Destroy(Tbx::IRenderer* rendererToDestroy)
    {
        delete rendererToDestroy;
    }

    std::string OpenGLRenderingModule::GetName() const
    {
        return "OpenGL Rendering";
    }

    std::string OpenGLRenderingModule::GetAuthor() const
    {
        return "Jeremy Hummel";
    }

    int OpenGLRenderingModule::GetVersion() const
    {
        return 0;
    }
}

Tbx::Module* Load()
{
    return new OpenGLRendering::OpenGLRenderingModule();
}

void Unload(Tbx::Module* moduleToUnload)
{
    delete moduleToUnload;
}