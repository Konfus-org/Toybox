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
    // Ownership: Owns a Scope<Message> clone and result metadata until processing completes.
    // Thread-safety: Not thread-safe; synchronized externally by AppMessageCoordinator.
    struct TBX_API AppQueuedMessage
    {
        AppQueuedMessage() = default;
        ~AppQueuedMessage() = default;

        AppQueuedMessage(const AppQueuedMessage&) = delete;
        AppQueuedMessage& operator=(const AppQueuedMessage&) = delete;
        AppQueuedMessage(AppQueuedMessage&&) = default;
        AppQueuedMessage& operator=(AppQueuedMessage&&) = default;

        Scope<Message> message;
        Result result;
        Timer timer;
        std::chrono::steady_clock::time_point timeout_deadline;
    };

    // Thread-safe message coordinator handling synchronous dispatch and deferred processing for
    // the application module.
    // Ownership: Owns enqueued message copies until delivery; stores handlers by value.
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

        uuid add_handler(MessageHandler handler);
        void remove_handler(const uuid& token);
        void clear();

        Result send(Message& msg) const override;
        Result post(Message& msg) override;

        void process() override;

      private:
        void dispatch(Message& msg, std::chrono::steady_clock::time_point deadline = {}) const;

        std::vector<std::pair<uuid, MessageHandler>> _handlers;
        std::vector<AppQueuedMessage> _pending;

        mutable std::mutex _handlers_mutex;
        mutable std::mutex _queue_mutex;
    };
}
