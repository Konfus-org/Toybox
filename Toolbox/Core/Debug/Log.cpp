#include "TbxCorePCH.h"
#include "Log.h"
#include "DebugAPI.h"

namespace Tbx
{
    void Log::Open(const std::string& logSaveLocation)
    {
        auto event = OpenLogRequestEvent(logSaveLocation, _logName);
        Events::Send(event);
        TBX_ASSERT(event.Handled, "Failed to open the log {}, is a logger created and listening?", _logName);
    }

    void Log::Close()
    {
        auto event = CloseLogRequestEvent(_logName);
        Events::Send(event);
        TBX_ASSERT(event.Handled, "Failed to close the log {}, is a logger under that name created and listening?", _logName);
    }
}