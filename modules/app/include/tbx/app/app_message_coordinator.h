#pragma once
#include "tbx/common/uuid.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace tbx
{
    struct QueuedMessage
    {
        std::unique_ptr<Message> message;
        std::promise<Result> completion_state;
    };

    struct RegisteredMessageHandler
    {
        Uuid id = {};
        std::shared_ptr<MessageHandler> handler = nullptr;
    };

    class TBX_API AppMessageCoordinator final : public IMessageCoordinator
    {
      public:
        /// <summary>
        /// Purpose: Creates a message coordinator that dispatches immediate and queued messages.
        /// </summary>
        /// <remarks>
        /// Ownership: Owns registered handlers and queued message storage.
        /// Thread Safety: Safe for concurrent posting/sending with concurrent handler updates.
        /// </remarks>
        AppMessageCoordinator();

        /// <summary>
        /// Purpose: Flushes pending messages and releases all handlers during shutdown.
        /// </summary>
        /// <remarks>
        /// Ownership: Releases all owned handlers and queued messages before returning.
        /// Thread Safety: Must not race destruction; external synchronization required.
        /// </remarks>
        ~AppMessageCoordinator() noexcept override;

        AppMessageCoordinator(const AppMessageCoordinator&) = delete;
        AppMessageCoordinator& operator=(const AppMessageCoordinator&) = delete;
        AppMessageCoordinator(AppMessageCoordinator&&) = delete;
        AppMessageCoordinator& operator=(AppMessageCoordinator&&) = delete;

        using IMessageDispatcher::post;
        using IMessageDispatcher::send;

        Uuid register_handler(MessageHandler handler) override;
        void deregister_handler(const Uuid& token) override;
        void clear_handlers() override;

        void flush() override;

        Result send(Message& msg) const override;
        std::shared_future<Result> post(std::unique_ptr<Message> msg) const override;

      private:
        std::shared_ptr<const std::vector<RegisteredMessageHandler>> get_handlers_snapshot() const;
        void dispatch(Message& msg) const;

        mutable std::mutex _handlers_write_mutex;
        mutable std::shared_ptr<const std::vector<RegisteredMessageHandler>> _handlers_snapshot;
        mutable std::mutex _pending_mutex;
        mutable std::vector<QueuedMessage> _pending;
    };
}
