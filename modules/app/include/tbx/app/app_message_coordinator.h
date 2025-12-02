#pragma once
#include "tbx/common/smart_pointers.h"
#include "tbx/common/uuid.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/handler.h"
#include "tbx/tbx_api.h"
#include "tbx/time/timer.h"
#include <chrono>
#include <mutex>
#include <utility>
#include <vector>

namespace tbx
{
    // Internal storage wrapper used by AppMessageCoordinator to track queued messages before
    // dispatch.
    // Ownership: Holds an owning Scope of a copied Message. The queued message owns its callbacks
    // and shared result; callers retain ownership of their original messages and observe outcomes
    // through shared Result instances and callbacks.
    // Thread-safety: Not thread-safe; synchronized externally by AppMessageCoordinator.
    struct TBX_API AppQueuedMessage
    {
        AppQueuedMessage(const Message& source, Timer queue_timer, std::chrono::steady_clock::time_point deadline);
        ~AppQueuedMessage() = default;

        AppQueuedMessage(const AppQueuedMessage&) = delete;
        AppQueuedMessage& operator=(const AppQueuedMessage&) = delete;
        AppQueuedMessage(AppQueuedMessage&&) = default;
        AppQueuedMessage& operator=(AppQueuedMessage&&) = default;

        Scope<Message> message;
        Timer timer;
        std::chrono::steady_clock::time_point timeout_deadline;
    };

    // Thread-safe message coordinator handling synchronous dispatch and deferred processing for
    // the application module.
    // Ownership: Stores handlers by value and owns queued message copies, sharing Result state with
    // callers through the returned Result and any shared Message instances provided by callers.
    // Thread-safety: Coordinates access to handlers and pending messages with internal mutexes.
    class TBX_API AppMessageCoordinator final
        : public IMessageDispatcher
        , public IMessageProcessor
    {
      public:
        AppMessageCoordinator();
        ~AppMessageCoordinator() override;

        AppMessageCoordinator(const AppMessageCoordinator&) = delete;
        AppMessageCoordinator& operator=(const AppMessageCoordinator&) = delete;
        AppMessageCoordinator(AppMessageCoordinator&&) = delete;
        AppMessageCoordinator& operator=(AppMessageCoordinator&&) noexcept = delete;

        Uuid add_handler(MessageHandler handler);
        void remove_handler(const Uuid& token);
        void clear();

        Result send(Message& msg) const override;
        Result post(const Message& msg) override;

        void process() override;

      private:
        void dispatch(Message& msg, std::chrono::steady_clock::time_point deadline = {}) const;

        std::vector<std::pair<Uuid, MessageHandler>> _handlers;
        std::vector<AppQueuedMessage> _pending;

        mutable std::mutex _handlers_mutex;
        mutable std::mutex _queue_mutex;
    };
}
