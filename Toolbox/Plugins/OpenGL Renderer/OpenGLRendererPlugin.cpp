#include "OpenGLRendererPlugin.h"

namespace OpenGLRendering
{
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