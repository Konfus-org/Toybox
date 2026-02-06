#include "tbx/debugging/logging.h"
#include "tbx/debugging/log_requests.h"
#include <string>

namespace tbx
{
    static constexpr std::string_view STDOUT_FALLBACK_LOGGER_NAME = "StdoutLoggerPlugin";
    static constexpr std::string_view STDOUT_FALLBACK_LOGGER_DESCRIPTION =
        "Fallback logger plugin that writes to stdout.";
    static constexpr std::string_view STDOUT_FALLBACK_LOGGER_VERSION = "1.0.0";

    std::string Log::format(std::string_view message)
    {
        return std::string(message);
    }

    std::string Log::format(const char* message)
    {
        if (message == nullptr)
        {
            return std::string();
        }

        return std::string(message);
    }

    void Log::post(
        const IMessageDispatcher& dispatcher,
        LogLevel level,
        const char* file,
        int line,
        const std::string& message)
    {
        dispatcher.post<LogMessageRequest>(level, message, file, line);
    }

    std::string_view get_stdout_fallback_logger_name()
    {
        return STDOUT_FALLBACK_LOGGER_NAME;
    }

    std::string_view get_stdout_fallback_logger_description()
    {
        return STDOUT_FALLBACK_LOGGER_DESCRIPTION;
    }

    std::string_view get_stdout_fallback_logger_version()
    {
        return STDOUT_FALLBACK_LOGGER_VERSION;
    }
}
