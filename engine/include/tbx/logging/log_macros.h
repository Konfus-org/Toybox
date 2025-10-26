#pragma once
#include "tbx/logging/logging.h"

#define TBX_TRACE_INFO_EX(CmdDispatcherRef, ...)      ::tbx::submit_formatted((CmdDispatcherRef), ::tbx::LogLevel::Info, __FILE__, __LINE__, __VA_ARGS__)
#define TBX_TRACE_WARNING_EX(CmdDispatcherRef, ...)   ::tbx::submit_formatted((CmdDispatcherRef), ::tbx::LogLevel::Warning, __FILE__, __LINE__, __VA_ARGS__)
#define TBX_TRACE_ERROR_EX(CmdDispatcherRef, ...)     ::tbx::submit_formatted((CmdDispatcherRef), ::tbx::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
#define TBX_TRACE_CRITICAL_EX(CmdDispatcherRef, ...)  ::tbx::submit_formatted((CmdDispatcherRef), ::tbx::LogLevel::Critical, __FILE__, __LINE__, __VA_ARGS__)

#define TBX_TRACE_INFO(...)      ::tbx::submit_formatted(::tbx::LogLevel::Info, __FILE__, __LINE__, __VA_ARGS__)
#define TBX_TRACE_WARNING(...)   ::tbx::submit_formatted(::tbx::LogLevel::Warning, __FILE__, __LINE__, __VA_ARGS__)
#define TBX_TRACE_ERROR(...)     ::tbx::submit_formatted(::tbx::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
#define TBX_TRACE_CRITICAL(...)  ::tbx::submit_formatted(::tbx::LogLevel::Critical, __FILE__, __LINE__, __VA_ARGS__)

#define TBX_ASSERT_EX(CmdDispatcherRef, cond, ...)\
    do\
    {\
        if (!(cond))\
        {\
            ::tbx::submit_formatted((CmdDispatcherRef), ::tbx::LogLevel::Critical, __FILE__, __LINE__, __VA_ARGS__); \
        }\
    } while (0)

#define TBX_ASSERT(cond, ...)\
    do\
    {\
        if (!(cond))\
        {\
            ::tbx::submit_formatted(::tbx::LogLevel::Critical, __FILE__, __LINE__, __VA_ARGS__); \
        }\
    } while (0)
