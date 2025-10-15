#pragma once
#include "Tbx/Debug/LogLevel.h"
#include "Tbx/DllExport.h"
#include "Tbx/Events/Event.h"
#include <string>

namespace Tbx
{
    class TBX_EXPORT LogMessageEvent : public Event
    {
    public:
        LogMessageEvent(LogLevel level, const std::string& message);

        LogLevel GetLevel() const;
        const std::string& GetMessage() const;
        std::string TakeMessage();

        std::string ToString() const override;

    private:
        LogLevel _level = LogLevel::Info;
        std::string _message = {};
    };
}
