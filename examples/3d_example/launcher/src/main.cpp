#include "tbx/systems/app/application.h"

#ifdef TBX_DEBUG
static void debug_terminate_handler()
{
    std::abort();
}

static void init_crash_policy()
{
    std::set_terminate(debug_terminate_handler);
}
#else
static void init_debug_crash_policy() {}
#endif

// TODO: Make a standardized launcher and make an App hot reloadable. The launcher is Toybox owned
// and what the App is right now. The app then becomes client owned and is a specialized thing users
// can use to make apps or games (App is basically just a plugin, the launcher can coordinate many
// apps and plugins).
int main()
{
    init_crash_policy();
    TBX_TRY_CATCH_ASSERT(
        {
            auto app = tbx::Application();
            return app.run();
        },
        "Application error occured!");
}
