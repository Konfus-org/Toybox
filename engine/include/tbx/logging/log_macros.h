#pragma once
#include "tbx/logging/logging.h"

#define TBX_TRACE_INFO(CmdDispatcherRef, msg)      ::tbx::submit_log((CmdDispatcherRef), ::tbx::LogLevel::Info, __FILE__, __LINE__, (msg))
#define TBX_TRACE_WARNING(CmdDispatcherRef, msg)   ::tbx::submit_log((CmdDispatcherRef), ::tbx::LogLevel::Warning, __FILE__, __LINE__, (msg))
#define TBX_TRACE_ERROR(CmdDispatcherRef, msg)     ::tbx::submit_log((CmdDispatcherRef), ::tbx::LogLevel::Error, __FILE__, __LINE__, (msg))
#define TBX_TRACE_CRITICAL(CmdDispatcherRef, msg)  ::tbx::submit_log((CmdDispatcherRef), ::tbx::LogLevel::Critical, __FILE__, __LINE__, (msg))

#define TBX_ASSERT(CmdDispatcherRef, cond, msg)\
    do\
    {\
        if (!(cond))\
        {\
            ::tbx::submit_log((CmdDispatcherRef), ::tbx::LogLevel::Critical, __FILE__, __LINE__, (msg)); \
        }\
    } while (0)
