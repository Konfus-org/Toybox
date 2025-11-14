#pragma once

namespace tbx
{
    enum class TBX_API LogLevel
    {
        Info,
        Warning,
        Error,
        Critical
    };
    
    inline const char* to_string(LogLevel lvl)
    {
        switch (lvl)
        {
            case LogLevel::Info:
                return "INFO";
            case LogLevel::Warning:
                return "WARN";
            case LogLevel::Error:
                return "ERROR";
            case LogLevel::Critical:
                return "CRITICAL";
        }
        return "UNKNOWN";
    }
}