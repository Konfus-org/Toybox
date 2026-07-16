#include "launcher.h"
#include "tbx/systems/app/command_list.h"
#include "tbx/systems/debugging/macros.h"
#include <cstdlib>
#include <exception>
#include <string>

#ifdef TBX_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <cstdio>
    #include <windows.h>
#endif

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

#ifdef TBX_PLATFORM_WINDOWS

// The launcher is a GUI-subsystem app so windowed and editor-hosted runs never flash a console.
// A headless run has no window at all, so give it a console - the parent terminal if it was
// launched from one, otherwise a fresh one - and point stdio at it; without this the engine looks
// like it is doing nothing. Logs still always also reach the rotated TbxDebug.log file sink.
static void setup_headless_console(const tbx::CommandList& command_list)
{
    if (!command_list.has("headless"))
        return;

    if (AttachConsole(ATTACH_PARENT_PROCESS) == FALSE && AllocConsole() == FALSE)
        return;

    FILE* stream = nullptr;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    freopen_s(&stream, "CONIN$", "r", stdin);
}

#endif

int main(int argc, char* argv[])
{
    init_crash_policy();

#ifdef TBX_PLATFORM_WINDOWS
    setup_headless_console(tbx::CommandList(argc, argv));
#endif

    auto launcher = Launcher();
    return launcher.run(argc, argv);
}
