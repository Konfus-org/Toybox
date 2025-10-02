#pragma once
#include "Tbx/Debug/Log.h"

#ifdef TBX_ASSERTS_ENABLED
    /// <summary>
    /// Asserts that a condition is true, if it isn't this method will write a critical level msg to the log.
    /// Msg will be written to a log file in release and a console in debug.
    /// And if in debug and our assert failed (evaluated to false) the app will break into the debugger
    /// This is a good method to use to validate things.
    /// </summary>
    #if defined(TBX_PLATFORM_WINDOWS)
        #define TBX_DEBUG_BREAK() __debugbreak()
    #else
        #include <csignal>
        #define TBX_DEBUG_BREAK() std::raise(SIGTRAP)
    #endif
    #define TBX_ASSERT(check, ...) do { if(!(check)) { Tbx::Log::Critical(__VA_ARGS__); TBX_DEBUG_BREAK(); } } while(0)
#else
    /// <summary>
    /// Asserts that a condition is true, if it isn't this method sends a critical level msg to the log.
    /// Msg will be written to a log file in release and a console in debug.
    /// This is a good method to use to validate things.
    /// </summary>
    #define TBX_ASSERT(check, ...) do { if(!(check)) { Tbx::Log::Critical(__VA_ARGS__); } } while(0)
#endif

/// <summary>
/// Ensures a ptr is valid, if it isn't this method sends a critical level msg to the log.
/// If a log is created and listening the msg will be written to a log file in release, others a console in debug.
/// And if in debug and our assert failed (evaluated to false) the app will break into the debugger
/// </summary>
#define TBX_VALIDATE_PTR(ptr, ...) TBX_ASSERT((ptr) != nullptr, __VA_ARGS__)

/// <summary>
/// Ensures a weak ptr is valid, if it isn't this method sends a critical level msg to the log.
/// If a log is created and listening the msg will be written to a log file in release, others a console in debug.
/// And if in debug and our assert failed (evaluated to false) the app will break into the debugger
/// </summary>
#define TBX_VALIDATE_WEAK_PTR(ptr, ...) TBX_ASSERT((!ptr.expired() && ptr.lock() != nullptr), __VA_ARGS__)