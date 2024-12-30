#include "OpenGLRenderingModule.h"
#include "OpenGLRenderer.h"

namespace OpenGLRendering
{
    Toybox::IRenderer* OpenGLRenderingModule::Create()
    {
        return new OpenGLRenderer();
    }

    void OpenGLRenderingModule::Destroy(Toybox::IRenderer* rendererToDestroy)
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

Toybox::Module* Load()
{
    return new OpenGLRendering::OpenGLRenderingModule();
}

void Unload(Toybox::Module* moduleToUnload)
{
    delete moduleToUnload;
}