#pragma once
#include "InteropAPI.h"
#include "InteropApp.h"

static Toybox::Interop::InteropApp* EditorCoreApp;
Toybox::App* Toybox::CreateApp()
{
    return new Toybox::Interop::InteropApp();
}

TBX_EDITOR_CORE_API Toybox::uint64 LaunchViewport()
{
    if (EditorCoreApp == nullptr) 
    {
        EditorCoreApp = (Toybox::Interop::InteropApp*)Toybox::CreateApp();
        EditorCoreApp->Launch();
        EditorCoreApp->GetMainWindow()->SetMode(Toybox::WindowMode::Borderless);
    }
    return EditorCoreApp->GetMainWindow()->GetId();
}

TBX_EDITOR_CORE_API void UpdateViewport()
{
    if (EditorCoreApp != nullptr && EditorCoreApp->GetMainWindow() != nullptr)
    { 
        EditorCoreApp->Update();
    }
}

TBX_EDITOR_CORE_API void CloseViewport()
{
    if (EditorCoreApp != nullptr && EditorCoreApp->GetMainWindow() != nullptr)
    {
        EditorCoreApp->Close();
    }
}