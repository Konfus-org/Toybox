#include "tbx/messages/dispatcher_context.h"

namespace tbx
{
    static thread_local IMessageDispatcher* global_dispatcher = nullptr;

    IMessageDispatcher* current_dispatcher()
    {
        return global_dispatcher;
    }

    IMessageDispatcher* set_current_dispatcher(IMessageDispatcher* dispatcher)
    {
        IMessageDispatcher* prev = global_dispatcher;
        global_dispatcher = dispatcher;
        return prev;
    }
}
