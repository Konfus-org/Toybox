#pragma once
#include "Application/App.h"

extern Toybox::Application::App* Toybox::Application::CreateApp();

int main(int argc, char* argv[])
{
    Toybox::Application::App* app = Toybox::Application::CreateApp();
    app->Launch();
    while (app->IsRunning())
    {
        app->Update();
    }

    delete app;
    return 0;
}
