#include "tbx/dispatch/dispatcher_context.h"

namespace tbx
{
    static thread_local MessageDispatcher* global_dispatcher = nullptr;

    MessageDispatcher* current_dispatcher()
    {
        return global_dispatcher;
    }

    MessageDispatcher* set_current_dispatcher(MessageDispatcher* dispatcher)
    {
        MessageDispatcher* prev = global_dispatcher;
        global_dispatcher = dispatcher;
        return prev;
    }
}

