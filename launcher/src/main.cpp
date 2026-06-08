#include "launcher.h"
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

int main(int argc, char* argv[])
{
    init_crash_policy();
    auto launcher = Launcher();
    return launcher.run(argc, argv);
}
