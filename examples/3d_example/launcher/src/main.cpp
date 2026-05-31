#include "tbx/systems/app/application.h"

// TODO: Make a standardized launcher and make an App hot reloadable. The launcher is Toybox owned
// and what the App is right now. The app then becomes client owned and is a specialized thing users
// can use to make apps or games (App is basically just a plugin, the launcher can coordinate many
// apps and plugins).
int main()
{
    TBX_TRY_CATCH_ASSERT(
        {
            auto app = tbx::Application();
            return app.run();
        },
        "Application error occured!");
}
