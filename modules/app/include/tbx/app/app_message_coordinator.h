#pragma once
#include "tbx/async/lock.h"
#include "tbx/common/collections.h"
#include "tbx/common/pair.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/common/uuid.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    struct QueuedMessage
    {
        Scope<Message> message;
        Promise<Result> completion_state;
    };

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

        void process() override;

      private:
        Result send_impl(Message& msg) const override;
        Promise<Result> post_impl(Scope<Message> msg) const override;
        void dispatch(Message& msg) const;

        ThreadSafe<List<Pair<Uuid, MessageHandler>>> _handlers;
        ThreadSafe<List<QueuedMessage>> _pending;
    };
}
