#pragma once
#include "tbx/common/collections.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/common/uuid.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/tbx_api.h"
#include <future>
#include <memory>
#include <mutex>
#include <utility>

namespace tbx
{
    struct QueuedMessage
    {
        Scope<Message> message;
        Ref<std::promise<Result>> completion;
    };
    
    // Thread-safe message coordinator handling synchronous dispatch and deferred processing for
    // the application module.
    // Ownership: Stores handlers by value, constructs messages per send call, and owns queued
    // message copies for post().
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

        void process() override;

      private:
        Result send_impl(Message& msg) const override;
        std::future<Result> post_impl(Scope<Message> msg) const override;
        void dispatch(Message& msg) const;

        List<std::pair<Uuid, MessageHandler>> _handlers;
        mutable List<QueuedMessage> _pending;

        mutable std::mutex _handlers_mutex;
        mutable std::mutex _queue_mutex;
    };
}
