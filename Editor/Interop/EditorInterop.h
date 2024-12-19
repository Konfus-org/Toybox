#pragma once
#include "EditorCoreAPI.h"
#include "EditorCoreApp.h"

static Toybox::Editor::EditorCoreApp* EditorCoreApp;

// TODO: Make toybox support multi-window and instead of getting the main window, every time this is called we add a new window (viewport)
TBX_EDITOR_CORE_API Toybox::uint64 LaunchViewport()
{
    if (EditorCoreApp == nullptr) 
    {
        EditorCoreApp = new Toybox::Editor::EditorCoreApp();
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