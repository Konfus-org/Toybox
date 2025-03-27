#include "Toybox.h"

namespace Tbx
{
    void Run(App& app)
    {
        // Launch and run application
        app.Launch();
        while (app.IsRunning()) app.Update();
        app.Close();
    }
}