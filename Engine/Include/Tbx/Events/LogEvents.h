#pragma once
#include "Tbx/Debug/LogLevel.h"
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include <string>

namespace Tbx
{
    struct TBX_EXPORT LogMessageEvent : public Event
    {
        LogMessageEvent(LogLevel level, std::string message)
            : Level(level), Message(std::move(message)) {}

        const LogLevel Level = LogLevel::Info;
        const std::string Message = {};
    };
}
