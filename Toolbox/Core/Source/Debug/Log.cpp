#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Debug/DebugAPI.h"
#include "Tbx/Core/Events/EventDispatcher.h"

namespace Tbx
{
    void Log::Open(const std::string& logSaveLocation)
    {
        auto event = OpenLogRequestEvent(logSaveLocation, _logName);
        EventDispatcher::Send(event);
        TBX_ASSERT(event.IsHandled, "Failed to open the log {}, is a logger created and listening?", _logName);
    }

    void Log::Close()
    {
        auto event = CloseLogRequestEvent(_logName);
        EventDispatcher::Send(event);
        TBX_ASSERT(event.IsHandled, "Failed to close the log {}, is a logger under that name created and listening?", _logName);
    }
}