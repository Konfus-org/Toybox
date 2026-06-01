#include "tbx/systems/app/application.h"
#include "tbx/systems/debugging/macros.h"
#include <cstdlib>
#include <exception>
#include <string>

#ifdef TBX_DEBUG
static std::string get_terminate_reason()
{
    const auto exception = std::current_exception();
    if (!exception)
        return "Unhandled termination without an active exception.";

    try
    {
        std::rethrow_exception(exception);
    }
    catch (const std::exception& ex)
    {
        return std::string("Unhandled exception: ").append(ex.what());
    }
    catch (...)
    {
        return "Unhandled non-standard exception.";
    }
}

static void debug_terminate_handler()
{
    TBX_ASSERT(false, "{}", get_terminate_reason());
    std::abort();
}

static void init_crash_policy()
{
    std::set_terminate(debug_terminate_handler);
}
#else
static void init_crash_policy() {}
#endif

// TODO: Make a standardized launcher and make an App class that is more like a specialized plugin.
// The launcher is Toybox owned and what the App is right now. The app then becomes client owned and
// is a specialized thing users can use to make apps or games (App is basically just a plugin, the
// launcher can coordinate many apps and plugins).
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
