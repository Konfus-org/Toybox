#pragma once

#include "Tbx/Core/Debug/ILogger.h"
#include "Tbx/Core/Debug/Log.h"

#define TBX_TRACE(msg, ...)         Tbx::Log::Trace(msg, __VA_ARGS__)
#define TBX_INFO(msg, ...)          Tbx::Log::Info(msg, __VA_ARGS__)
#define TBX_WARN(msg, ...)          Tbx::Log::Warn(msg, __VA_ARGS__)
#define TBX_ERROR(msg, ...)         Tbx::Log::Error(msg, __VA_ARGS__)
#define TBX_CRITICAL(msg, ...)      Tbx::Log::Critical(msg, __VA_ARGS__)

#ifdef TBX_ASSERTS_ENABLED
    #define TBX_ASSERT(check, msg, ...) if(!(check)) TBX_CRITICAL(msg, __VA_ARGS__); if(!(check)) __debugbreak()
#else
    #define TBX_ASSERT(...)
#endif

#define TBX_VALIDATE_PTR(ptr, error_msg, ...)  TBX_ASSERT(ptr != nullptr, error_msg, __VA_ARGS__)
#define TBX_VALIDATE_WEAK_PTR(ptr, error_msg, ...)  TBX_ASSERT((!ptr.expired() && ptr.lock() != nullptr), error_msg, __VA_ARGS__)
