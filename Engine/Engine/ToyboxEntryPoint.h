#pragma once
#include "Application/App.h"
#include "Modules/Modules.h"

extern Toybox::App* Toybox::CreateApp();

int main(int argc, char* argv[])
{
    // Load modules
    Toybox::ModuleServer::LoadModules();

    // Create and run application
    Toybox::App* app = Toybox::CreateApp();
    app->Launch();
    while (app->IsRunning())
    {
        app->Update();
    }

    // Final cleanup
    Toybox::ModuleServer::UnloadModules();
    delete app;
    return 0;
}
