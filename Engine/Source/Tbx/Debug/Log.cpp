#include "Tbx/PCH.h"
#include "Tbx/Debug/Log.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Memory/Hashing.h"
#include "Tbx/Memory/Refs.h"
#include <cstdio>
#include <utility>

namespace Tbx
{
    namespace
    {
        bool logQueueWarningIssued = false;
    }

    void Log::Trace(LogLevel level, std::string message)
    {
        auto bus = EventBus::Global;
        if (!bus)
        {
            std::printf("Toybox: Missing global event bus, dropping log: %s\n", message.c_str());
            return;
        }

        const auto listeners = bus->GetCallbacks(Hash<LogMessageEvent>());
        if (listeners.empty() && !logQueueWarningIssued)
        {
            std::printf("Toybox: Queuing log messages without an active listener; logs will be dropped until a LogManager binds.\n");
            logQueueWarningIssued = true;
        }

        bus->QueueEvent(MakeExclusive<LogMessageEvent>(level, message));
    }
}
