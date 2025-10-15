#include "Tbx/PCH.h"
#include "Tbx/Debug/StdOutLogger.h"
#include "Tbx/Debug/LogLevel.h"
#include "Tbx/Time/CurrentTime.h"
#include <cstdio>

namespace Tbx
{
    static const char* ResolveLogLevelLabel(LogLevel level)
    {
        const char* label = "UNKNOWN";
        switch (level)
        {
            case LogLevel::Trace:
            {
                label = "TRACE";
            }
            break;
            case LogLevel::Debug:
            {
                label = "DEBUG";
            }
            break;
            case LogLevel::Info:
            {
                label = "INFO";
            }
            break;
            case LogLevel::Warn:
            {
                label = "WARN";
            }
            break;
            case LogLevel::Error:
            {
                label = "ERROR";
            }
            break;
            case LogLevel::Critical:
            {
                label = "CRITICAL";
            }
            break;
            default:
            {
                label = "UNKNOWN";
            }
            break;
        }

        return label;
    }

    void StdOutLogger::Open(const std::string& name, const std::string& filepath)
    {
        (void)name;
        (void)filepath;
    }

    void StdOutLogger::Close()
    {
        std::fflush(stdout);
    }

    void StdOutLogger::Flush()
    {
        std::fflush(stdout);
    }

    void StdOutLogger::Write(int lvl, const std::string& msg)
    {
        const char* levelLabel = ResolveLogLevelLabel(static_cast<LogLevel>(lvl));
        const auto timestamp = GetCurrentTimestamp();
        std::printf("[%s][%s] %s", timestamp.c_str(), levelLabel, msg.c_str());
    }
}
