#include "jolt_runtime_lifetime.h"
#include "tbx/core/systems/debugging/macros.h"
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/RegisterTypes.h>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace jolt_physics
{
    static std::size_t g_runtime_reference_count = 0U;
    static std::mutex g_runtime_mutex = {};

    static std::string format_jolt_trace_message(const char* fmt, std::va_list args)
    {
        if (fmt == nullptr || *fmt == '\0')
            return std::string("Jolt reported an empty trace message.");

        std::va_list args_copy;
        va_copy(args_copy, args);
        int required_chars = std::vsnprintf(nullptr, 0, fmt, args_copy);
        va_end(args_copy);
        if (required_chars <= 0)
            return std::string(fmt);

        auto message = std::string {};
        message.resize(static_cast<std::size_t>(required_chars));
        std::vsnprintf(
            message.data(),
            static_cast<std::size_t>(required_chars) + 1U,
            fmt,
            args);
        return message;
    }

    static void tbx_jolt_trace_callback(const char* fmt, ...)
    {
        std::va_list args;
        va_start(args, fmt);
        std::string message = format_jolt_trace_message(fmt, args);
        va_end(args);

        TBX_TRACE_INFO("Jolt: {}", message);
    }

#ifdef JPH_ENABLE_ASSERTS
    static bool tbx_jolt_assert_failed_callback(
        const char* expression,
        const char* message,
        const char* file,
        JPH::uint line)
    {
        const char* safe_expression =
            (expression && *expression) ? expression : "<expression unavailable>";
        const char* safe_message = (message && *message) ? message : "";
        const char* safe_file = (file && *file) ? file : "<unknown>";

        if (*safe_message == '\0')
        {
            TBX_TRACE_CRITICAL(
                "Jolt assertion failed: '{}' at {}:{}",
                safe_expression,
                safe_file,
                line);
        }
        else
        {
            TBX_TRACE_CRITICAL(
                "Jolt assertion failed: '{}' at {}:{} ({})",
                safe_expression,
                safe_file,
                line,
                safe_message);
        }

        return false;
    }
#endif

    bool JoltRuntimeLifetime::acquire()
    {
        const auto lock = std::scoped_lock(g_runtime_mutex);
        if (g_runtime_reference_count == 0U)
        {
            JPH::RegisterDefaultAllocator();
            JPH::Trace = tbx_jolt_trace_callback;
#ifdef JPH_ENABLE_ASSERTS
            JPH::AssertFailed = tbx_jolt_assert_failed_callback;
#endif

            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }

        ++g_runtime_reference_count;
        return true;
    }

    void JoltRuntimeLifetime::release()
    {
        const auto lock = std::scoped_lock(g_runtime_mutex);
        if (g_runtime_reference_count == 0U)
            return;

        --g_runtime_reference_count;
        if (g_runtime_reference_count > 0U)
            return;

        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}
