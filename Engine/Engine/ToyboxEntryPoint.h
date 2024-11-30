#pragma once
#include "Application/App.h"
#include "Modules/Modules.h"

extern Toybox::Application::App* Toybox::Application::CreateApp();

int main(int argc, char* argv[])
{
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
