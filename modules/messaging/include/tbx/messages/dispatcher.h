#pragma once
#include "tbx/messages/message.h"
#include "tbx/common/result.h"
#include "tbx/tbx_api.h"
#include <concepts>
#include <future>
#include <memory>
#include <type_traits>
#include <utility>

namespace tbx
{
    /// <summary>
    /// Purpose: Defines the interface for components that dispatch messages to the system.
    /// </summary>
    /// <remarks>
    /// Ownership: Constructed messages are owned by the dispatcher while executing; `post`
    /// takes ownership of a queued copy while `send` uses a stack instance.
    /// Thread Safety: Implementations may define their own guarantees. Unless documented
    /// otherwise by the concrete type, calls are expected from the engine's main thread.
    /// </remarks>
    class TBX_API IMessageDispatcher
    {
      public:
        virtual ~IMessageDispatcher() noexcept = default;

        /// <summary>
        /// Purpose: Immediately sends an existing message instance.
        /// </summary>
        /// <remarks>
        /// Ownership: The caller retains ownership; the dispatcher operates on the provided object.
        /// Thread Safety: See class notes.
        /// </remarks>
        template <typename TMessage>
            requires std::derived_from<TMessage, Message>
        Result send(TMessage& msg) const
        {
            return send_impl(static_cast<Message&>(msg));
        }

        /// <summary>
        /// Purpose: Immediately constructs and sends a message.
        /// </summary>
        /// <remarks>
        /// Ownership: The message is constructed in-place for the dispatch.
        /// Thread Safety: See class notes.
        /// </remarks>
        template <typename TMessage, typename... TArgs>
            requires(std::derived_from<TMessage, Message> && (sizeof...(TArgs) > 0)
                     && std::is_constructible_v<TMessage, TArgs...>)
        Result send(TArgs&&... args) const
        {
            TMessage msg(std::forward<TArgs>(args)...);
            return send_impl(msg);
        }

        /// <summary>
        /// Purpose: Enqueues an existing message instance for deferred processing.
        /// </summary>
        /// <remarks>
        /// Ownership: Copies the message for queued delivery.
        /// Thread Safety: See class notes.
        /// </remarks>
        template <typename TMessage>
            requires std::derived_from<TMessage, Message>
        std::shared_future<Result> post(const TMessage& msg) const
        {
            return post_impl(std::make_unique<TMessage>(msg));
        }

        /// <summary>
        /// Purpose: Enqueues a constructed message for deferred processing.
        /// </summary>
        /// <remarks>
        /// Ownership: Takes ownership of the queued copy until delivery. Returns a future that
        /// becomes ready once processing completes.
        /// Thread Safety: See class notes.
        /// </remarks>
        template <typename TMessage, typename... TArgs>
            requires(std::derived_from<TMessage, Message> && (sizeof...(TArgs) > 0)
                     && std::is_constructible_v<TMessage, TArgs...>)
        std::shared_future<Result> post(TArgs&&... args) const
        {
            return post_impl(std::make_unique<TMessage>(std::forward<TArgs>(args)...));
        }

      protected:
        virtual Result send_impl(Message& msg) const = 0;
        virtual std::shared_future<Result> post_impl(std::unique_ptr<Message> msg) const = 0;
    };

    /// <summary>
    /// Purpose: Defines the interface for components that advance posted work, typically
    /// driven once per frame by the host.
    /// </summary>
    /// <remarks>
    /// Ownership: Implementations own their queued messages and handler registrations.
    /// Thread Safety: Not required to be thread-safe; expected single-thread usage unless
    /// documented otherwise by an implementation.
    /// </remarks>
    class TBX_API IMessageQueue
    {
      public:
        virtual ~IMessageQueue() noexcept = default;

        /// <summary>
        /// Purpose: Processes all posted messages.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not transfer ownership of queued message storage.
        /// Thread Safety: See class notes.
        /// </remarks>
        virtual void flush() = 0;
    };

    /// <summary>
    /// Purpose: Defines the interface for components that register and remove message handlers.
    /// </summary>
    /// <remarks>
    /// Ownership: Implementations own stored handlers and their lifetimes.
    /// Thread Safety: Implementations define their own guarantees.
    /// </remarks>
    class TBX_API IMessageHandlerRegistrar
    {
      public:
        virtual ~IMessageHandlerRegistrar() noexcept = default;

        /// <summary>
        /// Purpose: Registers a new message handler and returns its token.
        /// </summary>
        /// <remarks>
        /// Ownership: The registrar stores the handler by value.
        /// Thread Safety: See class notes.
        /// </remarks>
        virtual Uuid add_handler(MessageHandler handler) = 0;

        /// <summary>
        /// Purpose: Removes a previously registered handler by token.
        /// </summary>
        /// <remarks>
        /// Ownership: The registrar releases its stored handler for the token.
        /// Thread Safety: See class notes.
        /// </remarks>
        virtual void remove_handler(const Uuid& token) = 0;

        /// <summary>
        /// Purpose: Clears all registered handlers.
        /// </summary>
        /// <remarks>
        /// Ownership: The registrar releases all stored handlers.
        /// Thread Safety: See class notes.
        /// </remarks>
        virtual void clear_handlers() = 0;
    };

    /// <summary>
    /// Purpose: Returns the current global dispatcher pointer (may be nullptr).
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning. The setter retains ownership and must ensure the dispatcher
    /// outlives its use through this API.
    /// Thread Safety: Thread-safe. Uses an atomic pointer to read the global dispatcher
    /// without additional synchronization.
    /// </remarks>
    TBX_API IMessageDispatcher* get_global_dispatcher();

    /// <summary>
    /// Purpose: Sets the current dispatcher, returning the previous value.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning. The caller retains ownership and must ensure the dispatcher
    /// outlives all use through this API.
    /// Thread Safety: Thread-safe. Uses an atomic pointer to exchange the dispatcher value
    /// for the process.
    /// </remarks>
    TBX_API IMessageDispatcher* set_global_dispatcher(IMessageDispatcher* dispatcher);

    /// <summary>
    /// Purpose: Sets the current global dispatcher for the lifetime of the scope, restoring
    /// the previous value when destroyed.
    /// </summary>
    /// <remarks>
    /// Ownership: Non-owning. The caller retains ownership and must ensure the dispatcher
    /// outlives the scope where it is set.
    /// Thread Safety: Thread-safe for setting and restoring the global dispatcher pointer.
    /// The dispatcher instance itself must remain valid for the lifetime of the scope.
    /// </remarks>
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

        ~GlobalDispatcherScope() noexcept
        {
            set_global_dispatcher(_prev);
        }

        GlobalDispatcherScope(const GlobalDispatcherScope&) = delete;
        GlobalDispatcherScope& operator=(const GlobalDispatcherScope&) = delete;

      private:
        IMessageDispatcher* _prev = nullptr;
    };
}
