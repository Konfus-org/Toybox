#pragma once
#include "tbx/common/uuid.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"
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

    class TBX_API AppMessageCoordinator final
        : public IMessageDispatcher
        , public IMessageQueue
        , public IMessageHandlerRegistrar
    {
      public:
        AppMessageCoordinator();
        ~AppMessageCoordinator() noexcept override;

        AppMessageCoordinator(const AppMessageCoordinator&) = delete;
        AppMessageCoordinator& operator=(const AppMessageCoordinator&) = delete;
        AppMessageCoordinator(AppMessageCoordinator&&) = delete;
        AppMessageCoordinator& operator=(AppMessageCoordinator&&) = delete;

        Uuid add_handler(MessageHandler handler) override;
        void remove_handler(const Uuid& token) override;
        void clear_handlers() override;

        void flush() override;

      private:
        Result send_impl(Message& msg) const override;
        std::shared_future<Result> post_impl(std::unique_ptr<Message> msg) const override;
        void dispatch(Message& msg) const;

        mutable std::mutex _handlers_mutex;
        std::vector<std::pair<Uuid, MessageHandler>> _handlers;
        mutable std::mutex _pending_mutex;
        mutable std::vector<QueuedMessage> _pending;
    };
}
