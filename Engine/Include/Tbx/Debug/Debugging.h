#pragma once
#include "Tbx/Debug/Log.h"
#include "Tbx/Debug/Asserts.h"

/// <summary>
/// Writes a trace level msg to the log.
/// If a log is created and listening the msg will be written to a log file in release, a console in debug.
/// This is a good method to use to add debug info to the log to track down bugs.
/// </summary>
//#define TBX_TRACE(...) Tbx::Log::Trace(__VA_ARGS__)

/// <summary>
/// Writes a debug level msg to the log.
/// Msg will be written to a log file in release and a console in debug.
/// This is a good method to use to log info that is intended to be used to track down a bug with the intention to be removed once the bug is solved.
/// </summary>
#define TBX_TRACE_DEBUG(...) Tbx::Log::Debug(__VA_ARGS__)

/// <summary>
/// Writes a info level msg to the log.
/// Msg will be written to a log file in release and a console in debug.
/// This is a good method to use to log info to the log that we want to track during runtime. Ex: window resize, layers attached, shutdown triggered, etc...
/// </summary>
#define TBX_TRACE_INFO(...) Tbx::Log::Info(__VA_ARGS__)

/// <summary>
/// Writes a warning to the log.
/// Msg will be written to a log file in release and a console in debug.
/// This is a good method to use to warn about something that might be problematic, but can likely be ignored and app execution can continue without crashing.
/// I.e. errors we can recover from.
/// </summary>
#define TBX_TRACE_WARNING(...) Tbx::Log::Warn(__VA_ARGS__)

/// <summary>
/// Writes an error level msg to the log.
/// Msg will be written to a log file in release and a console in debug.
/// This is a good method to use to log errors that will likely cause issues during runtime.
/// </summary>
#define TBX_TRACE_ERROR(...) Tbx::Log::Error(__VA_ARGS__)

/// <summary>
/// Writes a critical level msg to the log.
/// Msg will be written to a log file in release and a console in debug.
/// This is a good method to use if the app is in a state where it cannot continue i.e. unrecoverable errors.
/// </summary>
#define TBX_TRACE_CRITICAL(...) Tbx::Log::Critical(__VA_ARGS__)

#ifdef TBX_VERBOSE_LOGGING
    /// <summary>
    /// Logs verbose info for debugging IF verbose logging is enabled.
    /// Verbose logging is enabled.
    /// </summary>
    #define TBX_TRACE_VERBOSE(...) Tbx::Log::Info(__VA_ARGS__)
#else
    /// <summary>
    /// Logs verbose info for debugging IF verbose logging is enabled.
    /// Verbose logging is disabled.
    /// </summary>
    #define TBX_TRACE_VERBOSE(...)
#endif

