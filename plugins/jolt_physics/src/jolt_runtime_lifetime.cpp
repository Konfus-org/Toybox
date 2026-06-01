#include "jolt_runtime_lifetime.h"
#include "tbx/systems/debugging/macros.h"
// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/RegisterTypes.h>
// clang-format on
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace jolt_physics
{
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
        std::vsnprintf(message.data(), static_cast<std::size_t>(required_chars) + 1U, fmt, args);
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

    class JoltRuntimeState final
    {
      public:
        static JoltRuntimeState& get_instance()
        {
            static JoltRuntimeState state = {};
            return state;
        }

      public:
        JoltRuntimeState(const JoltRuntimeState&) = delete;
        JoltRuntimeState& operator=(const JoltRuntimeState&) = delete;
        JoltRuntimeState(JoltRuntimeState&&) = delete;
        JoltRuntimeState& operator=(JoltRuntimeState&&) = delete;

      public:
        bool acquire()
        {
            const auto lock = std::scoped_lock(_runtime_mutex);
            if (_runtime_reference_count == 0U)
            {
                JPH::RegisterDefaultAllocator();
                JPH::Trace = tbx_jolt_trace_callback;
#ifdef JPH_ENABLE_ASSERTS
                JPH::AssertFailed = tbx_jolt_assert_failed_callback;
#endif

                JPH::Factory::sInstance = new JPH::Factory();
                JPH::RegisterTypes();
            }

            ++_runtime_reference_count;
            return true;
        }

        void release()
        {
            const auto lock = std::scoped_lock(_runtime_mutex);
            if (_runtime_reference_count == 0U)
                return;

            --_runtime_reference_count;
            if (_runtime_reference_count > 0U)
                return;

            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }

      private:
        JoltRuntimeState() = default;
        ~JoltRuntimeState() noexcept = default;

      private:
        std::mutex _runtime_mutex = {};
        std::size_t _runtime_reference_count = 0U;
    };

    bool JoltRuntimeLifetime::acquire()
    {
        return JoltRuntimeState::get_instance().acquire();
    }

    void JoltRuntimeLifetime::release()
    {
        JoltRuntimeState::get_instance().release();
    }
}
