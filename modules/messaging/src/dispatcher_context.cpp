#include "tbx/messages/dispatcher.h"

namespace tbx
{
    static IMessageDispatcher* global_dispatcher = nullptr;

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
