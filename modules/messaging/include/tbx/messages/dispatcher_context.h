#pragma once
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // Returns the current global dispatcher pointer (may be nullptr).
    // Ownership: non-owning. The setter retains ownership and must ensure the
    // dispatcher outlives its use through this API.
    // Thread-safety: Not thread-safe. Intended for use on a single thread
    // (usually the application's main loop) where the dispatcher is swapped
    // in and out via DispatcherScope.
    TBX_API IMessageDispatcher* current_dispatcher();

    // Sets the current dispatcher, returning the previous value.
    // Ownership: non-owning. The caller retains ownership and must ensure the
    // dispatcher outlives all use through this API.
    // Thread-safety: Not thread-safe; see current_dispatcher.
    TBX_API IMessageDispatcher* set_current_dispatcher(IMessageDispatcher* dispatcher);

    // RAII helper that sets the current thread-local dispatcher for the lifetime of the scope,
    // restoring the previous value when destroyed.
    // Ownership: non-owning. The caller retains ownership and must ensure the
    // dispatcher outlives the scope where it is set.
    // Thread-safety: Not thread-safe; intended for single-threaded usage.
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
