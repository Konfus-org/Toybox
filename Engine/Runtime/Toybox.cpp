#include "Toybox.h"

namespace Toybox
{
    void Run(App& app)
    {
        // Create and run application
        app.Launch();
        while (app.IsRunning())
        {
            app.Update();
        }
    }
}