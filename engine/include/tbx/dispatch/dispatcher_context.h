#pragma once
#include <cstddef>

namespace tbx
{
    class MessageDispatcher;

    // Returns the thread-local current dispatcher (may be nullptr).
    MessageDispatcher* current_dispatcher();

    // Sets the thread-local current dispatcher, returns previous value.
    MessageDispatcher* set_current_dispatcher(MessageDispatcher* dispatcher);

    // RAII helper that sets the current dispatcher for the lifetime of the scope.
    // Dispatcher is local to the thread this scope is created on.
    class DispatcherScope
    {
    public:
        explicit DispatcherScope(MessageDispatcher* dispatcher)
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
        MessageDispatcher* _prev = nullptr;
    };
}

