#pragma once
#include "tbx/core/api.h"
#include <format>
#include <string_view>

// The one logging seam: the selected backend (cmake tbx_backend(LOGGING ...)) implements
// write_log in its own .cpp — swapped at link time like every other backend. Nothing else
// names the library; formatting happens here with std::format.
namespace tbx
{
    /// @brief
    /// Purpose: Severity of one log line. (FAIL, not ERROR — windows.h steals that name.)
    enum class LogLevel : int
    {
        TRACE = 0,
        INFO,
        WARN,
        FAIL
    };

    /// @brief
    /// Purpose: Emits one formatted line; implemented by the selected logging backend.
    TBX_API void write_log(LogLevel level, std::string_view message);

    /// @brief
    /// Purpose: Logs at trace level with std::format-style formatting.
    template <typename... Args>
    void log_trace(std::format_string<Args...> fmt, Args&&... args)
    {
        write_log(LogLevel::TRACE, std::format(fmt, std::forward<Args>(args)...));
    }

    /// @brief
    /// Purpose: Logs at info level with std::format-style formatting.
    template <typename... Args>
    void log_info(std::format_string<Args...> fmt, Args&&... args)
    {
        write_log(LogLevel::INFO, std::format(fmt, std::forward<Args>(args)...));
    }

    /// @brief
    /// Purpose: Logs at warn level with std::format-style formatting.
    template <typename... Args>
    void log_warn(std::format_string<Args...> fmt, Args&&... args)
    {
        write_log(LogLevel::WARN, std::format(fmt, std::forward<Args>(args)...));
    }

    /// @brief
    /// Purpose: Logs at error level with std::format-style formatting.
    template <typename... Args>
    void log_error(std::format_string<Args...> fmt, Args&&... args)
    {
        write_log(LogLevel::FAIL, std::format(fmt, std::forward<Args>(args)...));
    }
}

#ifdef TBX_ASSERTS_ENABLED
    #define TBX_ASSERT(condition, ...)                                                             \
        do                                                                                         \
        {                                                                                          \
            if (!(condition))                                                                      \
            {                                                                                      \
                ::tbx::log_error("assert failed: " __VA_ARGS__);                                   \
                std::abort();                                                                      \
            }                                                                                      \
        } while (false)
#else
    #define TBX_ASSERT(condition, ...) ((void)0)
#endif
