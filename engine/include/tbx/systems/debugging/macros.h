#pragma once
#include "tbx/systems/debugging/logging.h"
#include <cstdlib>
#include <exception>

#define TBX_TRACE_FLUSH() ::tbx::Log::get_instance().flush()

// Token-paste helpers for generating a unique identifier per macro expansion.
#define TBX_CONCAT_IMPL(a, b) a##b
#define TBX_CONCAT(a, b) TBX_CONCAT_IMPL(a, b)

// Pushes a log category (see Log::begin_category) for the rest of the enclosing scope, so every line
// logged within it is tagged "[Category]"; it pops automatically on scope exit (RAII). Use this at call
// sites instead of the raw tbx::LogCategoryScope so it can be compiled out wholesale — like the other
// gated log endpoints — by not defining TBX_ENABLE_LOG_CATEGORIES (then it costs nothing and never
// evaluates its argument).
#if defined(TBX_ENABLE_LOG_CATEGORIES)
    #define TBX_LOG_CATEGORY_SCOPE(category)                                                       \
        ::tbx::LogCategoryScope TBX_CONCAT(tbx_log_category_scope_, __LINE__) { (category) }
#else
    #define TBX_LOG_CATEGORY_SCOPE(category) ((void)0)
#endif

#define TBX_TRACE_INFO(msg, ...)                                                                   \
    do                                                                                             \
    {                                                                                              \
        ::tbx::Log::get_instance().write(                                                          \
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

#ifdef TBX_ASSERTS_ENABLED
    #define TBX_ASSERT_FAILURE_TERMINATE()                                                         \
        do                                                                                         \
        {                                                                                          \
            TBX_TRACE_FLUSH();                                                                     \
            TBX_DEBUG_BREAK();                                                                     \
            std::abort();                                                                          \
        } while (0)
#else
    #define TBX_ASSERT_FAILURE_TERMINATE()                                                         \
        do                                                                                         \
        {                                                                                          \
            TBX_TRACE_FLUSH();                                                                     \
        } while (0)
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
            TBX_ASSERT_FAILURE_TERMINATE();                                                        \
        }                                                                                          \
    } while (0)

#define TBX_TRY_CATCH_ASSERT(to_try, failure_msg)                                                  \
    do                                                                                             \
    {                                                                                              \
        try                                                                                        \
        {                                                                                          \
            to_try                                                                                 \
        }                                                                                          \
        catch (const std::exception& ex)                                                           \
        {                                                                                          \
            TBX_ASSERT(false, "{}\nException:\n{}", failure_msg, ex.what());                       \
        }                                                                                          \
        catch (...)                                                                                \
        {                                                                                          \
            TBX_ASSERT(false, "{}\nUnknown exception...", failure_msg);                            \
        }                                                                                          \
    } while (0)
