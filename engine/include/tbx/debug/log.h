#pragma once
#include "tbx/utils/api.h"
#include <format>
#include <string_view>
#include <utility>

// The one logging seam: the selected backend (cmake tbx_backend(LOGGING ...)) implements
// write_log in its own .cpp — swapped at link time like every other backend. Nothing else
// names the library; formatting happens here with std::format.
//
// Log through the TBX_TRACE/TBX_INFO/TBX_WARN/TBX_ERROR macros: they stamp the source
// file/line onto every message, and levels below TBX_LOG_LEVEL compile out entirely.
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
    /// Purpose: Emits one formatted line (with its source location); implemented by the
    /// selected logging backend.
    TBX_API void write_log(
        LogLevel level,
        std::string_view message,
        std::string_view source_file,
        int source_line);

    /// @brief
    /// Purpose: Formats and forwards one log line — the TBX_* macros' engine; call through
    /// them so builds can strip levels.
    template <typename... TArgs>
    void log_message(
        const LogLevel level,
        const char* source_file,
        const int source_line,
        std::format_string<TArgs...> format,
        TArgs&&... args)
    {
        write_log(
            level, std::format(format, std::forward<TArgs>(args)...), source_file, source_line);
    }
}

// The compiled-in floor: 0 keeps everything, 1 drops TRACE, 2 drops INFO, 3 keeps only
// errors. Override with -DTBX_LOG_LEVEL=N per build.
#ifndef TBX_LOG_LEVEL
    #ifdef TBX_DEBUG
        #define TBX_LOG_LEVEL 0
    #else
        #define TBX_LOG_LEVEL 1
    #endif
#endif

#if TBX_LOG_LEVEL <= 0
    #define TBX_TRACE(...)                                                                         \
        ::tbx::log_message(::tbx::LogLevel::TRACE, __FILE__, __LINE__, __VA_ARGS__)
#else
    #define TBX_TRACE(...) ((void)0)
#endif

#if TBX_LOG_LEVEL <= 1
    #define TBX_INFO(...)                                                                          \
        ::tbx::log_message(::tbx::LogLevel::INFO, __FILE__, __LINE__, __VA_ARGS__)
#else
    #define TBX_INFO(...) ((void)0)
#endif

#if TBX_LOG_LEVEL <= 2
    #define TBX_WARN(...)                                                                          \
        ::tbx::log_message(::tbx::LogLevel::WARN, __FILE__, __LINE__, __VA_ARGS__)
#else
    #define TBX_WARN(...) ((void)0)
#endif

#if TBX_LOG_LEVEL <= 3
    #define TBX_ERROR(...)                                                                         \
        ::tbx::log_message(::tbx::LogLevel::FAIL, __FILE__, __LINE__, __VA_ARGS__)
#else
    #define TBX_ERROR(...) ((void)0)
#endif

#ifdef TBX_ASSERTS_ENABLED
    #define TBX_ASSERT(condition, ...)                                                             \
        do                                                                                         \
        {                                                                                          \
            if (!(condition))                                                                      \
            {                                                                                      \
                TBX_ERROR("assert failed: " __VA_ARGS__);                                          \
                std::abort();                                                                      \
            }                                                                                      \
        } while (false)
#else
    #define TBX_ASSERT(condition, ...) ((void)0)
#endif
