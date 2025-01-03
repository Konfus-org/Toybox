#include "EditorInterop.h"
#include "EditorCoreApp.h"
#include "EditorLayer.h"

namespace Tbx::Editor
{
    EditorCoreApp::EditorCoreApp() : App("Tbx")
    {
        PushLayer(new EditorLayer("Main Layer"));
    }
}