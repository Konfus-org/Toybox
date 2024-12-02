#pragma once
#include "Application/App.h"
#include "Modules/Modules.h"

extern Toybox::App* AppInstance;
extern Toybox::App* Toybox::GetAppInstance()
{
    return AppInstance;
}

extern Toybox::App* Toybox::CreateApp();

int main(int argc, char* argv[])
{
    // Load modules
    Toybox::ModuleServer::LoadModules();

    // Create and run application
    AppInstance = Toybox::CreateApp();
    AppInstance->Launch();
    while (AppInstance->IsRunning())
    {
        AppInstance->Update();
    }

    // Final cleanup
    Toybox::ModuleServer::UnloadModules();
    delete AppInstance;
    return 0;
}
