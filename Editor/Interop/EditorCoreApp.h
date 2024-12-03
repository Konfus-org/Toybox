#pragma once
#include <Toybox.h>
#include "EditorCoreApp.h"
#include "EditorCoreAPI.h"

namespace Toybox::Editor
{
    class EditorCoreApp : public App
    {
    public:
        EditorCoreApp();
        ~EditorCoreApp() = default;
    };
}