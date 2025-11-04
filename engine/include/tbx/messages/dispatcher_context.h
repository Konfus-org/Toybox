#pragma once
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // Returns the global thread safe current dispatcher interface (may be nullptr).
    // Ownership: setter of the dispatcher owns it.
    TBX_API IMessageDispatcher* current_dispatcher();

    // Sets the global thread safe current dispatcher interface, returns previous value.
    // Ownership: this does not take ownership; callers must ensure the
    // dispatcher outlives the scope where it is set.
    TBX_API IMessageDispatcher* set_current_dispatcher(IMessageDispatcher* dispatcher);

    // RAII helper that sets the current thread-local dispatcher for the lifetime of the scope,
    // restoring the previous value when destroyed.
    // Ownership: this does not take ownership; callers must ensure the
    // dispatcher outlives the scope where it is set.
    class TBX_API DispatcherScope
    {
       public:
        DispatcherScope(IMessageDispatcher& dispatcher)
            : _prev(set_current_dispatcher(&dispatcher))
        {
        }
        DispatcherScope(IMessageDispatcher* dispatcher)
            : _prev(set_current_dispatcher(dispatcher))
        {
        }

        ~DispatcherScope()
        {
            set_current_dispatcher(_prev);
        }

        DispatcherScope(const DispatcherScope&) = delete;
        DispatcherScope& operator=(const DispatcherScope&) = delete;

       private:
        IMessageDispatcher* _prev = nullptr;
    };
}
