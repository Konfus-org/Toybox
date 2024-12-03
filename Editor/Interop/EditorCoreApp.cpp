#include "EditorInterop.h"
#include "EditorCoreApp.h"
#include "EditorLayer.h"

namespace Toybox::Editor
{
    EditorCoreApp::EditorCoreApp() : App("Toybox")
    {
        PushLayer(new EditorLayer("Main Layer"));
    }
}