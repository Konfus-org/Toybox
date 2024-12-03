#include "Interop.h"
#include "InteropApp.h"
#include "MainLayer.h"

namespace Toybox::Interop
{
    InteropApp::InteropApp() : App("Toybox")
    {
        PushLayer(new EditorLayer("Main Layer"));
    }
}