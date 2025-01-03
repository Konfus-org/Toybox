#pragma once
#include <Tbx.h>
#include "EditorCoreApp.h"
#include "EditorCoreAPI.h"

namespace Tbx::Editor
{
    class EditorCoreApp : public App
    {
    public:
        EditorCoreApp();
        ~EditorCoreApp() = default;
    };
}