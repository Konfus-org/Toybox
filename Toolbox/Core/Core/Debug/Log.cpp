#include "Core/ToolboxPCH.h"
#include "Core/Debug/DebugAPI.h"
#include "Core/Event Dispatcher/EventDispatcher.h"

namespace Tbx
{
    void Log::Open(const std::string& logSaveLocation)
    {
        auto event = OpenLogRequestEvent(logSaveLocation, _logName);
        EventDispatcher::Send(event);
        TBX_ASSERT(event.Handled, "Failed to open the log {}, is a logger created and listening?", _logName);
    }

    void Log::Close()
    {
        auto event = CloseLogRequestEvent(_logName);
        EventDispatcher::Send(event);
        TBX_ASSERT(event.Handled, "Failed to close the log {}, is a logger under that name created and listening?", _logName);
    }
}