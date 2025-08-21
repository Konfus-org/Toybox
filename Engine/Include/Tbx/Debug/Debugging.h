#pragma once
#include "Tbx/Debug/Log.h"

//#define TBX_VERBOSE_ENABLED

/// <summary>
/// Writes a trace level msg to the log.
/// If a log is created and listening the msg will be written to a log file in release, a console in debug.
/// This is a good method to use to add debug info to the log to track down bugs.
/// </summary>
#define TBX_TRACE(msg, ...)         Tbx::Log::Trace(msg, __VA_ARGS__)

/// <summary>
/// Writes a debug level msg to the log.
    /// Msg will be written to a log file in release and a console in debug.
/// This is a good method to use to log info that is intended to be used to track down a bug with the intention to be removed once the bug is solved.
/// </summary>
#define TBX_TRACE_DEBUG(msg, ...)  Tbx::Log::Debug(msg, __VA_ARGS__)

/// <summary>
/// Writes a info level msg to the log.
    /// Msg will be written to a log file in release and a console in debug.
/// This is a good method to use to log info to the log that we want to track during runtime. Ex: window resize, layers attached, shutdown triggered, etc...
/// </summary>
#define TBX_TRACE_INFO(msg, ...)          Tbx::Log::Info(msg, __VA_ARGS__)

/// <summary>
/// Writes a warning to the log.
    /// Msg will be written to a log file in release and a console in debug.
/// This is a good method to use to warn about something that might be problematic, but can likely be ignored and app execution can continue without crashing.
/// I.e. errors we can recover from.
/// </summary>
#define TBX_TRACE_WARNING(msg, ...)          Tbx::Log::Warn(msg, __VA_ARGS__)

/// <summary>
/// Sends an error level msg to the log.
    /// Msg will be written to a log file in release and a console in debug.
/// This is a good method to use to log errors that will likely cause issues during runtime.
/// </summary>
#define TBX_TRACE_ERROR(msg, ...)         Tbx::Log::Error(msg, __VA_ARGS__)

/// <summary>
/// Sends a critical level msg to the log.
    /// Msg will be written to a log file in release and a console in debug.
/// This is a good method to use if the app is in a state where it cannot continue i.e. unrecoverable errors.
/// </summary>
#define TBX_TRACE_CRITICAL(msg, ...)      Tbx::Log::Critical(msg, __VA_ARGS__)

#ifdef TBX_VERBOSE_ENABLED
    /// <summary>
    /// Logs verbose info for debugging IF verbose logging is enabled.
    /// Verbose logging is enabled.
    /// </summary>
    #define TBX_TRACE_VERBOSE(msg, ...)   Tbx::Log::Info(msg, __VA_ARGS__)
#else
    /// <summary>
    /// Logs verbose info for debugging IF verbose logging is enabled.
    /// Verbose logging is disabled.
    /// </summary>
    #define TBX_TRACE_VERBOSE(msg, ...)
#endif

#ifdef TBX_ASSERTS_ENABLED
    /// <summary>
    /// Asserts that a condition is true, if it isn't this method will write a critical level msg to the log.
    /// Msg will be written to a log file in release and a console in debug.
    /// And if in debug and our assert failed (evaluated to false) the app will break into the debugger
    /// This is a good method to use to validate things.
    /// </summary>
    #define TBX_ASSERT(check, msg, ...) if(!(check)) TBX_TRACE_CRITICAL(msg, __VA_ARGS__); if(!(check)) __debugbreak()
#else
    /// <summary>
    /// Asserts that a condition is true, if it isn't this method sends a critical level msg to the log.
    /// Msg will be written to a log file in release and a console in debug.
    /// This is a good method to use to validate things.
    /// </summary>
    #define TBX_ASSERT(check, msg, ...) if(!(check)) TBX_TRACE_CRITICAL(msg, __VA_ARGS__);
#endif

/// <summary>
/// Ensures a ptr is valid, if it isn't this method sends a critical level msg to the log.
/// If a log is created and listening the msg will be written to a log file in release, others a console in debug.
/// And if in debug and our assert failed (evaluated to false) the app will break into the debugger
/// </summary>
#define TBX_VALIDATE_PTR(ptr, error_msg, ...)  TBX_ASSERT(ptr != nullptr, error_msg, __VA_ARGS__)

/// <summary>
/// Ensures a weak ptr is valid, if it isn't this method sends a critical level msg to the log.
/// If a log is created and listening the msg will be written to a log file in release, others a console in debug.
/// And if in debug and our assert failed (evaluated to false) the app will break into the debugger
/// </summary>
#define TBX_VALIDATE_WEAK_PTR(ptr, error_msg, ...)  TBX_ASSERT((!ptr.expired() && ptr.lock() != nullptr), error_msg, __VA_ARGS__)