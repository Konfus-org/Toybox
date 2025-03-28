#include "OpenGLRendererPlugin.h"
#include "OpenGLRenderer.h"

namespace OpenGLRendering
{
    Tbx::IRenderer* OpenGLRendererPlugin::Provide()
    {
        return new OpenGLRenderer();
    }

    void OpenGLRendererPlugin::Destroy(Tbx::IRenderer* rendererToDestroy)
    {
        delete rendererToDestroy;
    }

    void OpenGLRendererPlugin::OnLoad()
    {
        // Rendering doesn't need to do anything on load
    }

    void OpenGLRendererPlugin::OnUnload()
    {
        // Rendering doesn't need to do anything on unload
    }
}

TBX_REGISTER_PLUGIN(OpenGLRendering::OpenGLRendererPlugin);