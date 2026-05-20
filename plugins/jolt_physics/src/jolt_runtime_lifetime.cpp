#include "jolt_runtime_lifetime.h"
#include "internal/jolt_runtime_lifetime_internal.h"
#include "tbx/systems/debugging/macros.h"
// clang-format off
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/RegisterTypes.h>
// clang-format on
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace jolt_physics
{
#ifdef JPH_ENABLE_ASSERTS
#endif

    bool JoltRuntimeLifetime::acquire()
    {
        const auto lock = std::scoped_lock(internal::g_runtime_mutex);
        if (internal::g_runtime_reference_count == 0U)
        {
            JPH::RegisterDefaultAllocator();
            JPH::Trace = internal::tbx_jolt_trace_callback;
#ifdef JPH_ENABLE_ASSERTS
            JPH::AssertFailed = internal::tbx_jolt_assert_failed_callback;
#endif

            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }

        ++internal::g_runtime_reference_count;
        return true;
    }

    void JoltRuntimeLifetime::release()
    {
        const auto lock = std::scoped_lock(internal::g_runtime_mutex);
        if (internal::g_runtime_reference_count == 0U)
            return;

        --internal::g_runtime_reference_count;
        if (internal::g_runtime_reference_count > 0U)
            return;

        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}
