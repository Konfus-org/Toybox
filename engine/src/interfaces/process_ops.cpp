#include "tbx/interfaces/process_ops.h"

#if defined(TBX_PLATFORM_WINDOWS)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_MACOS)
    #include <cerrno>
    #include <csignal>
#endif

namespace tbx
{
    bool is_process_running(uint32 pid)
    {
        if (pid == 0U)
            return false;

#if defined(TBX_PLATFORM_WINDOWS)
        // SYNCHRONIZE is enough to wait on the handle; a zero timeout just probes its state.
        const HANDLE handle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
        if (handle == nullptr)
            return false;

        const DWORD wait = WaitForSingleObject(handle, 0);
        CloseHandle(handle);
        return wait == WAIT_TIMEOUT;
#elif defined(TBX_PLATFORM_LINUX) || defined(TBX_PLATFORM_MACOS)
        // Signal 0 performs the permission/existence checks without delivering a signal.
        if (kill(static_cast<pid_t>(pid), 0) == 0)
            return true;

        // EPERM means the process exists but we may not signal it; only ESRCH means it is gone.
        return errno == EPERM;
#else
        return true;
#endif
    }
}
