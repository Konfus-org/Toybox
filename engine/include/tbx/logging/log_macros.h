#pragma once
#include "tbx/logging/logging.h"

#define TBX_TRACE_INFO_EX(CmdDispatcherRef, msg)      ::tbx::submit_log((CmdDispatcherRef), ::tbx::LogLevel::Info, __FILE__, __LINE__, (msg))
#define TBX_TRACE_WARNING_EX(CmdDispatcherRef, msg)   ::tbx::submit_log((CmdDispatcherRef), ::tbx::LogLevel::Warning, __FILE__, __LINE__, (msg))
#define TBX_TRACE_ERROR_EX(CmdDispatcherRef, msg)     ::tbx::submit_log((CmdDispatcherRef), ::tbx::LogLevel::Error, __FILE__, __LINE__, (msg))
#define TBX_TRACE_CRITICAL_EX(CmdDispatcherRef, msg)  ::tbx::submit_log((CmdDispatcherRef), ::tbx::LogLevel::Critical, __FILE__, __LINE__, (msg))

#define TBX_TRACE_INFO(msg)      ::tbx::submit_log(::tbx::LogLevel::Info, __FILE__, __LINE__, (msg))
#define TBX_TRACE_WARNING(msg)   ::tbx::submit_log(::tbx::LogLevel::Warning, __FILE__, __LINE__, (msg))
#define TBX_TRACE_ERROR(msg)     ::tbx::submit_log(::tbx::LogLevel::Error, __FILE__, __LINE__, (msg))
#define TBX_TRACE_CRITICAL(msg)  ::tbx::submit_log(::tbx::LogLevel::Critical, __FILE__, __LINE__, (msg))

#define TBX_ASSERT_EX(CmdDispatcherRef, cond, msg)\
    do\
    {\
        if (!(cond))\
        {\
            ::tbx::submit_log((CmdDispatcherRef), ::tbx::LogLevel::Critical, __FILE__, __LINE__, (msg)); \
        }\
    } while (0)

#define TBX_ASSERT(cond, msg)\
    do\
    {\
        if (!(cond))\
        {\
            ::tbx::submit_log(::tbx::LogLevel::Critical, __FILE__, __LINE__, (msg)); \
        }\
    } while (0)
