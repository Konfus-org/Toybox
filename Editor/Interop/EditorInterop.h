#pragma once
#include "EditorCoreAPI.h"

static Toybox::Editor::EditorCoreApp* EditorCoreApp;
Toybox::App* Toybox::CreateApp()
{
    return new Toybox::Editor::EditorCoreApp();
}

// TODO: Make toybox support multi-window and instead of getting the main window, every time this is called we add a new window (viewport)
TBX_EDITOR_CORE_API Toybox::uint64 LaunchViewport()
{
    if (EditorCoreApp == nullptr) 
    {
        EditorCoreApp = (Toybox::Editor::EditorCoreApp*)Toybox::CreateApp();
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