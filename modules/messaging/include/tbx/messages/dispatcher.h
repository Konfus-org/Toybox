#pragma once
#include "tbx/common/smart_pointers.h"
#include "tbx/async/promise.h"
#include "tbx/messages/message.h"
#include "tbx/messages/result.h"
#include "tbx/tbx_api.h"
#include <concepts>
#include <type_traits>
#include <utility>

namespace tbx
{
    // Interface for components that can dispatch messages to the system.
    // Ownership: Constructed messages are owned by the dispatcher while executing.
    // `post` takes ownership of a queued copy; `send` uses a stack instance.
    // Thread-safety: Implementations may define their own guarantees. Unless
    // documented otherwise by the concrete type, calls are expected from the
    // engine's main thread.
    class TBX_API IMessageDispatcher
    {
      public:
        virtual ~IMessageDispatcher() = default;

        // Immediately sends an existing message instance.
        // Ownership: caller retains ownership; dispatcher operates on the provided object.
        template <typename TMessage>
            requires std::derived_from<TMessage, Message>
        Result send(TMessage& msg) const
        {
            return send_impl(static_cast<Message&>(msg));
        }

        // Immediately constructs and sends a message.
        // Ownership: message is constructed in-place for the dispatch.
        // Thread-safety: see class notes.
        template <typename TMessage, typename... TArgs>
            requires(std::derived_from<TMessage, Message> && (sizeof...(TArgs) > 0)
                     && std::is_constructible_v<TMessage, TArgs...>)
        Result send(TArgs&&... args) const
        {
            TMessage msg(std::forward<TArgs>(args)...);
            return send_impl(msg);
        }

        // Enqueues an existing message instance for deferred processing.
        // Ownership: copies the message for queued delivery.
        // Thread-safety: see class notes.
        template <typename TMessage>
            requires std::derived_from<TMessage, Message>
        Promise<Result> post(const TMessage& msg) const
        {
            return post_impl(Scope<Message>(new TMessage(msg)));
        }

        // Enqueues a constructed message for deferred processing.
        // Ownership: takes ownership of the queued copy until delivery.
        // Returns a future that becomes ready once processing completes.
        // Thread-safety: see class notes.
        template <typename TMessage, typename... TArgs>
            requires(std::derived_from<TMessage, Message> && (sizeof...(TArgs) > 0)
                     && std::is_constructible_v<TMessage, TArgs...>)
        Promise<Result> post(TArgs&&... args) const
        {
            return post_impl(Scope<Message>(new TMessage(std::forward<TArgs>(args)...)));
        }

      protected:
        virtual Result send_impl(Message& msg) const = 0;
        virtual Promise<Result> post_impl(Scope<Message> msg) const = 0;
    };

    // Interface for components that advance queued work and deliver
    // posted messages. Typically driven once per frame/tick by the host.
    // Thread-safety: Not required to be thread-safe; expected single-thread
    // usage unless documented otherwise by an implementation.
    class TBX_API IMessageProcessor
    {
      public:
        virtual ~IMessageProcessor() = default;

        // processes all posted messages.
        virtual void process() = 0;
    };

    // Returns the current global dispatcher pointer (may be nullptr).
    // Ownership: non-owning. The setter retains ownership and must ensure the
    // dispatcher outlives its use through this API.
    // Thread-safety: Thread-safe. Uses an atomic pointer to read the global
    // dispatcher without additional synchronization.
    TBX_API IMessageDispatcher* get_global_dispatcher();

    // Sets the current dispatcher, returning the previous value.
    // Ownership: non-owning. The caller retains ownership and must ensure the
    // dispatcher outlives all use through this API.
    // Thread-safety: Thread-safe. Uses an atomic pointer to exchange the
    // dispatcher value for the process.
    TBX_API IMessageDispatcher* set_global_dispatcher(IMessageDispatcher* dispatcher);

    // RAII helper that sets the current global dispatcher for the lifetime of the scope,
    // restoring the previous value when destroyed.
    // Ownership: non-owning. The caller retains ownership and must ensure the
    // dispatcher outlives the scope where it is set.
    // Thread-safety: Thread-safe for setting and restoring the global
    // dispatcher pointer. The dispatcher instance itself must remain valid for
    // the lifetime of the scope.
    class TBX_API GlobalDispatcherScope
    {
      public:
        GlobalDispatcherScope(IMessageDispatcher& dispatcher)
            : _prev(set_global_dispatcher(&dispatcher))
        {
        }
        GlobalDispatcherScope(IMessageDispatcher* dispatcher)
            : _prev(set_global_dispatcher(dispatcher))
        {
        }

        ~GlobalDispatcherScope()
        {
            set_global_dispatcher(_prev);
        }

        GlobalDispatcherScope(const GlobalDispatcherScope&) = delete;
        GlobalDispatcherScope& operator=(const GlobalDispatcherScope&) = delete;

      private:
        IMessageDispatcher* _prev = nullptr;
    };
}
