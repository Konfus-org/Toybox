#include <atomic>
#include "tbx/messages/dispatcher.h"

namespace tbx
{
    static std::atomic<IMessageDispatcher*> global_dispatcher = nullptr;

    IMessageDispatcher* get_global_dispatcher()
    {
        return global_dispatcher.load();
    }

    IMessageDispatcher* set_global_dispatcher(IMessageDispatcher* dispatcher)
    {
        return global_dispatcher.exchange(dispatcher);
    }
}
