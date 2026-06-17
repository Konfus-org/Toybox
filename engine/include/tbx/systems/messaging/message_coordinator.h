#pragma once
#include "tbx/interfaces/message_dispatcher.h"

namespace tbx
{
    class TBX_API MessageCoordinator final : public IMessageCoordinator
    {
      public:
        using IMessageDispatcher::post;
        using IMessageDispatcher::send;

        MessageCoordinator();
        ~MessageCoordinator() noexcept override;

        MessageCoordinator(const MessageCoordinator&) = delete;
        MessageCoordinator& operator=(const MessageCoordinator&) = delete;
        MessageCoordinator(MessageCoordinator&&) = delete;
        MessageCoordinator& operator=(MessageCoordinator&&) = delete;

        Uuid register_handler(MessageHandler handler) override;
        void deregister_handler(const Uuid& token) override;
        void clear_handlers() override;

        void flush() override;

        Result send(Message& msg) const override;
        std::shared_future<Result> post(std::unique_ptr<Message> msg) const override;

      private:
        void dispatch(Message& msg) const;

      private:
        struct State;

        std::unique_ptr<State> _state = {};
    };
}
