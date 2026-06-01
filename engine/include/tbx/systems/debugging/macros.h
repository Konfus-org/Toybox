#pragma once
#include "tbx/systems/debugging/logging.h"

#define TBX_TRACE_FLUSH() ::tbx::Log::get_instance().flush()

#define TBX_TRACE_INFO(msg, ...)                                                                   \
    do                                                                                             \
    {                                                                                              \
        ::tbx::Log::get_instance().write(                                                         \
            ::tbx::LogLevel::INFO,                                                                 \
            __FILE__,                                                                              \
            __LINE__,                                                                              \
            msg __VA_OPT__(, ) __VA_ARGS__);                                                       \
    } while (0)

#define TBX_TRACE_INFO_ONCE(msg, ...)                                                              \
    do                                                                                             \
    {                                                                                              \
        ::tbx::Log::get_instance().write_once(                                                     \
            ::tbx::LogLevel::INFO,                                                                 \
            __FILE__,                                                                              \
            __LINE__,                                                                              \
            msg __VA_OPT__(, ) __VA_ARGS__);                                                       \
    } while (0)

#if defined(TBX_ENABLE_VERBOSE)
    #define TBX_TRACE_VERBOSE(msg, ...)                                                            \
        do                                                                                         \
        {                                                                                          \
            ::tbx::Log::get_instance().write(                                                      \
                ::tbx::LogLevel::INFO,                                                             \
                __FILE__,                                                                          \
                __LINE__,                                                                          \
                msg __VA_OPT__(, ) __VA_ARGS__);                                                   \
        } while (0)
#else
    #define TBX_TRACE_VERBOSE(msg, ...)                                                            \
        do                                                                                         \
        {                                                                                          \
        } while (0)
#endif

#define TBX_TRACE_WARNING(msg, ...)                                                                \
    do                                                                                             \
    {                                                                                              \
        ::tbx::Log::get_instance().write(                                                          \
            ::tbx::LogLevel::WARNING,                                                              \
            __FILE__,                                                                              \
            __LINE__,                                                                              \
            msg __VA_OPT__(, ) __VA_ARGS__);                                                       \
    } while (0)

#define TBX_TRACE_WARNING_ONCE(msg, ...)                                                           \
    do                                                                                             \
    {                                                                                              \
        ::tbx::Log::get_instance().write_once(                                                     \
            ::tbx::LogLevel::WARNING,                                                              \
            __FILE__,                                                                              \
            __LINE__,                                                                              \
            msg __VA_OPT__(, ) __VA_ARGS__);                                                       \
    } while (0)

#define TBX_TRACE_ERROR(msg, ...)                                                                  \
    do                                                                                             \
    {                                                                                              \
        ::tbx::Log::get_instance().write(                                                          \
            ::tbx::LogLevel::ERROR,                                                                \
            __FILE__,                                                                              \
            __LINE__,                                                                              \
            msg __VA_OPT__(, ) __VA_ARGS__);                                                       \
    } while (0)

#define TBX_TRACE_ERROR_ONCE(msg, ...)                                                             \
    do                                                                                             \
    {                                                                                              \
        ::tbx::Log::get_instance().write_once(                                                     \
            ::tbx::LogLevel::ERROR,                                                                \
            __FILE__,                                                                              \
            __LINE__,                                                                              \
            msg __VA_OPT__(, ) __VA_ARGS__);                                                       \
    } while (0)

#define TBX_TRACE_CRITICAL(msg, ...)                                                               \
    do                                                                                             \
    {                                                                                              \
        ::tbx::Log::get_instance().write(                                                          \
            ::tbx::LogLevel::CRITICAL,                                                             \
            __FILE__,                                                                              \
            __LINE__,                                                                              \
            msg __VA_OPT__(, ) __VA_ARGS__);                                                       \
    } while (0)

#define TBX_TRACE_CRITICAL_ONCE(msg, ...)                                                          \
    do                                                                                             \
    {                                                                                              \
        ::tbx::Log::get_instance().write_once(                                                     \
            ::tbx::LogLevel::CRITICAL,                                                             \
            __FILE__,                                                                              \
            __LINE__,                                                                              \
            msg __VA_OPT__(, ) __VA_ARGS__);                                                       \
    } while (0)

#ifdef TBX_ASSERTS_ENABLED
    // Asserts that a condition is true, if it isn't this method will write a critical level msg to
    // the log. Msg will be written to a log file in release and a console in debug. And if in debug
    // and our assert failed (evaluated to false) the app will break into the debugger This is a
    // good method to use to validate things.
    #if defined(TBX_PLATFORM_WINDOWS)
        #define TBX_DEBUG_BREAK() __debugbreak()
    #else
        #include <csignal>
        #define TBX_DEBUG_BREAK() std::raise(SIGTRAP)
    #endif
#else
    // Asserts that a condition is true, if it isn't this method sends a critical level msg to the
    // log. Msg will be written to a log file in release and a console in debug. This is a good
    // method to use to validate things.
    #define TBX_DEBUG_BREAK()
#endif

#define TBX_ASSERT(cond, ...)                                                                      \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            ::tbx::Log::get_instance().write(                                                      \
                ::tbx::LogLevel::CRITICAL,                                                         \
                __FILE__,                                                                          \
                __LINE__,                                                                          \
                __VA_ARGS__);                                                                      \
            TBX_DEBUG_BREAK();                                                                     \
        }                                                                                          \
    } while (0)

#define TBX_TRY_CATCH_ASSERT(to_try, failure_msg)                                                  \
    do                                                                                             \
    {                                                                                              \
        try                                                                                        \
        {                                                                                          \
            to_try                                                                                 \
        }                                                                                          \
        catch (std::exception ex)                                                                  \
        {                                                                                          \
            TBX_ASSERT(false, "{}\nException:\n{}", failure_msg, ex.what());                       \
        }                                                                                          \
        catch (...)                                                                                \
        {                                                                                          \
            TBX_ASSERT(false, "{}\nUnkown Exception...");                                          \
        }                                                                                          \
    } while (0)
