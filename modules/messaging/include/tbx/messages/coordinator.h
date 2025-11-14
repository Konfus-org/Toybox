#pragma once
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/handler.h"
#include "tbx/tbx_api.h"
#include "tbx/time/timer.h"
#include "tbx/std/list.h"
#include "tbx/std/smart_pointers.h"
#include "tbx/std/uuid.h"
#include <chrono>
#include <mutex>
#include <utility>

namespace tbx
{
    struct TBX_API QueuedMessage
    {
        QueuedMessage() = default;
        ~QueuedMessage() = default;

        QueuedMessage(const QueuedMessage&) = delete;
        QueuedMessage& operator=(const QueuedMessage&) = delete;
        QueuedMessage(QueuedMessage&&) = default;
        QueuedMessage& operator=(QueuedMessage&&) = default;

        tbx::Scope<Message> message;
        Result result;
        Timer timer;
        std::chrono::steady_clock::time_point timeout_deadline;
        Message* original = nullptr;
    };

    // Thread-safe message coordinator handling synchronous dispatch and deferred processing.
    // Ownership: Owns enqueued message copies until delivery; stores handlers by value.
    class TBX_API MessageCoordinator final
        : public IMessageDispatcher
        , public IMessageProcessor
    {
      public:
        MessageCoordinator();
        ~MessageCoordinator() override;

        MessageCoordinator(const MessageCoordinator&) = delete;
        MessageCoordinator& operator=(const MessageCoordinator&) = delete;
        MessageCoordinator(MessageCoordinator&&) = delete;
        MessageCoordinator& operator=(MessageCoordinator&&) noexcept = delete;

        Uuid add_handler(MessageHandler handler);
        void remove_handler(const Uuid& token);
        void clear();

        Result send(Message& msg) const override;
        Result post(const Message& msg) override;

        void process() override;

      private:
        void dispatch(Message& msg, std::chrono::steady_clock::time_point deadline = {}) const;

        List<std::pair<Uuid, MessageHandler>> _handlers;
        List<QueuedMessage> _pending;

        mutable std::mutex _handlers_mutex;
        mutable std::mutex _queue_mutex;
    };
}
