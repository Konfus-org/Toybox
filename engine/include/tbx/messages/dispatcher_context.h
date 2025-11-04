#pragma once
#include "tbx/application_context.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // RAII helper that sets the current thread-local application context for the lifetime of the
    // scope, restoring the previous value when destroyed. Ownership: this does not take ownership;
    // callers must ensure the context outlives the scope where it is set.
    class TBX_API AppScope
    {
       public:
        explicit AppScope(const ApplicationContext& context)
            : AppScope(&context)
        {
        }
        explicit AppScope(const ApplicationContext* context)
            : _prev_context(set_current_application_context(context))
        {
        }

        ~AppScope()
        {
            set_current_application_context(_prev_context);
        }

        AppScope(const AppScope&) = delete;
        AppScope& operator=(const AppScope&) = delete;

       private:
        const ApplicationContext* _prev_context = nullptr;
    };
}
