#include "tbx/interfaces/message_dispatcher.h"
#include <atomic>

namespace tbx
{
    static std::atomic<IMessageDispatcher*> global_dispatcher = nullptr;

    std::optional<std::reference_wrapper<IMessageDispatcher>> get_global_dispatcher()
    {
        auto& dispatcher = global_dispatcher;
        auto* current = dispatcher.load();
        if (!current)
            return std::nullopt;

        return std::ref(*current);
    }

    std::optional<std::reference_wrapper<IMessageDispatcher>> set_global_dispatcher(
        std::optional<std::reference_wrapper<IMessageDispatcher>> dispatcher)
    {
        auto* replacement = dispatcher.has_value() ? &dispatcher->get() : nullptr;
        auto* previous = global_dispatcher.exchange(replacement);
        if (!previous)
            return std::nullopt;

        return std::ref(*previous);
    }
}
