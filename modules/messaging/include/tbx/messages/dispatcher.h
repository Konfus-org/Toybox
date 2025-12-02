#pragma once
#include "tbx/messages/message.h"
#include "tbx/messages/result.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // Interface for components that can dispatch messages to the system.
    // Ownership: does not take ownership of messages; `send` reads the
    // message by reference, `post` copies it for deferred processing.
    // Thread-safety: Implementations may define their own guarantees. Unless
    // documented otherwise by the concrete type, calls are expected from the
    // engine's main thread.
    class TBX_API IMessageDispatcher
    {
      public:
        virtual ~IMessageDispatcher() = default;

        // Immediately sends a message by reference.
        // Ownership: non-owning; the caller keeps ownership of `msg`.
        // Thread-safety: see class notes.
        virtual Result send(Message& msg) const = 0;

        // Enqueues a copy of the message for deferred processing.
        // Ownership: copies `msg` internally until delivered.
        // Thread-safety: see class notes.
        virtual Result post(const Message& msg) = 0;
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
