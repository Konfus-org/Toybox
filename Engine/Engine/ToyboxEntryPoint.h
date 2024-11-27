#pragma once
#include "Application/App.h"
#include "Modules/Modules.h"

extern Toybox::Application::App* Toybox::Application::CreateApp();

int main(int argc, char* argv[])
{
    // Create module server to prompt load of modules and setting of module server singleton
    Toybox::Modules::ModuleServer moduleServer;

    // Create and run application
    Toybox::Application::App* app = Toybox::Application::CreateApp();
    app->Launch();
    while (app->IsRunning())
    {
        app->Update();
    }

    // Final cleanup
    delete app;
    return 0;
}
