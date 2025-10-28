#pragma once
#include "tbx/logging/logging.h"

#define TBX_TRACE_INFO_EX(CmdDispatcherRef, ...)      ::tbx::submit_formatted((CmdDispatcherRef), ::tbx::LogLevel::Info, __FILE__, __LINE__, __VA_ARGS__)
#define TBX_TRACE_WARNING_EX(CmdDispatcherRef, ...)   ::tbx::submit_formatted((CmdDispatcherRef), ::tbx::LogLevel::Warning, __FILE__, __LINE__, __VA_ARGS__)
#define TBX_TRACE_ERROR_EX(CmdDispatcherRef, ...)     ::tbx::submit_formatted((CmdDispatcherRef), ::tbx::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
#define TBX_TRACE_CRITICAL_EX(CmdDispatcherRef, ...)  ::tbx::submit_formatted((CmdDispatcherRef), ::tbx::LogLevel::Critical, __FILE__, __LINE__, __VA_ARGS__)

#define TBX_TRACE_INFO(msg, ...)      ::tbx::submit_formatted(::tbx::LogLevel::Info, __FILE__, __LINE__, msg, __VA_ARGS__)
#define TBX_TRACE_WARNING(msg, ...)   ::tbx::submit_formatted(::tbx::LogLevel::Warning, __FILE__, __LINE__, msg, __VA_ARGS__)
#define TBX_TRACE_ERROR(msg, ...)     ::tbx::submit_formatted(::tbx::LogLevel::Error, __FILE__, __LINE__, msg, __VA_ARGS__)
#define TBX_TRACE_CRITICAL(msg, ...)  ::tbx::submit_formatted(::tbx::LogLevel::Critical, __FILE__, __LINE__, msg, __VA_ARGS__)

#ifdef TBX_ASSERTS_ENABLED
    // Asserts that a condition is true, if it isn't this method will write a critical level msg to the log.
    // Msg will be written to a log file in release and a console in debug.
    // And if in debug and our assert failed (evaluated to false) the app will break into the debugger
    // This is a good method to use to validate things.
    #if defined(TBX_PLATFORM_WINDOWS)
        #define TBX_DEBUG_BREAK() __debugbreak()
    #else
        #include <csignal>
        #define TBX_DEBUG_BREAK() std::raise(SIGTRAP)
    #endif
#else
    // Asserts that a condition is true, if it isn't this method sends a critical level msg to the log.
    // Msg will be written to a log file in release and a console in debug.
    // This is a good method to use to validate things.
    #define TBX_DEBUG_BREAK()
#endif

#define TBX_ASSERT_EX(CmdDispatcherRef, cond, ...)\
    do\
    {\
        if (!(cond))\
        {\
            ::tbx::submit_formatted((CmdDispatcherRef), ::tbx::LogLevel::Critical, __FILE__, __LINE__, __VA_ARGS__); \
            TBX_DEBUG_BREAK(); \
        }\
    } while (0)

#define TBX_ASSERT(cond, ...)\
    do\
    {\
        if (!(cond))\
        {\
            ::tbx::submit_formatted(::tbx::LogLevel::Critical, __FILE__, __LINE__, __VA_ARGS__); \
            TBX_DEBUG_BREAK(); \
        }\
    } while (0)
