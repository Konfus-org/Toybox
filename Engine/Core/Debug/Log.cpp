#include "TbxPCH.h"
#include "Log.h"
#include "DebugAPI.h"

namespace Tbx
{
    void Log::Open(const std::string& name, const std::string& logSaveLocation)
    {
        auto event = OpenLogEvent(name, logSaveLocation);
        Events::Send(event);
        TBX_ASSERT(event.Handled, "Failed to open the log {}, is a logger created and listening?", name);
    }

    void Log::Close(const std::string& name)
    {
        auto event = CloseLogEvent(name);
        Events::Send(event);
        TBX_ASSERT(event.Handled, "Failed to close the log {}, is a logger under that name created and listening?", name);
    }
}