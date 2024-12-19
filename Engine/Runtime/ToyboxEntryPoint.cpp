#include "ToyboxEntryPoint.h"
#include "ToyboxAPI.h"

// API to create app, meant to be defined in CLIENT!
//extern Toybox::App * Toybox::CreateApp();

int main(int argc, char* argv[])
{
    // Load modules
    Toybox::ModuleServer::LoadModules();

    // Create and run application
    auto* app = Toybox::CreateApp();
    app->Launch();
    while (app->IsRunning())
    {
        app->Update();
    }

    // Final cleanup (order important, do not change!)
    delete app;

    Toybox::Input::StopHandling();
    Toybox::Log::Close();
    Toybox::ModuleServer::UnloadModules();

    return 0;
}