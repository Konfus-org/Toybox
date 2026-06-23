#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/tbx_api.h"
#include "tbx/types/uuid.h"
#include "tbx/utils/result.h"
#include <functional>
#include <string>
#include <string_view>

namespace tbx
{
    // The JSON-RPC error code reported when a handler's Result fails. Matches the studio bridge's
    // apply-failed code so a router-routed failure looks identical to a directly-sent one.
    constexpr int RPC_APPLY_FAILED_CODE = -32001;

    // Standard JSON-RPC protocol error codes, shared by the transport (parse/method-not-found) and
    // consumer handlers (invalid-params).
    constexpr int RPC_PARSE_ERROR_CODE = -32700;
    constexpr int RPC_METHOD_NOT_FOUND_CODE = -32601;
    constexpr int RPC_INVALID_PARAMS_CODE = -32602;

    /// @brief
    /// Purpose: Handed to an RPC handler so it can reply to one request without touching the
    /// transport. The router/host implements it, binding the request id and the outbound channel.
    /// @details
    /// Ownership: Borrowed for the duration of a single dispatch; a handler must not retain it.
    /// Thread Safety: Used on the dispatch thread only.
    class TBX_API RpcResponder
    {
      public:
        RpcResponder();
        virtual ~RpcResponder() noexcept;

      public:
        /// @brief Replies with a success result payload (pass an empty object for "no data").
        virtual void result(const Json& reply) = 0;

        /// @brief Replies with a JSON-RPC error (code + human-readable message).
        virtual void error(int code, std::string_view message) = 0;

        /// @brief True when the request is a notification (no id): a reply would be dropped, so
        /// handlers can skip building one.
        virtual bool is_notification() const = 0;

        /// @brief Convenience: replies with `reply` on success, or an apply-failed error carrying the
        /// result's report on failure. Expressed in terms of the primitives above.
        void respond(const Result& outcome, const Json& reply = Json::object());
    };

    // A handler for one RPC method: given the request params and a responder, does its work and
    // replies through the responder (or not, for a notification).
    using RpcHandler = std::function<void(const Json& params, RpcResponder& responder)>;

    /// @brief
    /// Purpose: A registry that routes incoming RPC method names to handlers. Any plugin can resolve
    /// it from the service provider and register methods, so RPC surface is no longer owned by a
    /// single plugin.
    /// @details
    /// Ownership: Owned by whichever plugin publishes it (the WindowsRPC plugin); consumers hold a
    /// weak_ptr and must deregister their methods on detach. Thread Safety: Registration and dispatch
    /// happen on the host's main thread; not safe for concurrent use.
    class TBX_API IRpcRouter
    {
      public:
        IRpcRouter();
        virtual ~IRpcRouter() noexcept;

      public:
        /// @brief Registers `handler` for `method`, owned by `owner` (the registering plugin's instance
        /// id, used to bulk-deregister and to arbitrate collisions). Succeeds when the method is newly
        /// registered or re-registered by the same owner (handler replaced, hot-reload friendly); fails
        /// when a different owner already holds it (the existing handler is kept untouched).
        virtual Result register_method(
            std::string_view method,
            RpcHandler handler,
            Uuid owner) = 0;

        /// @brief Removes `method` if it is owned by `owner`. Returns true if a handler was removed.
        virtual bool unregister_method(std::string_view method, Uuid owner) = 0;

        /// @brief Removes every method owned by `owner`. Call from a consumer's on_detach so the router
        /// never holds a closure capturing freed plugin memory.
        virtual void unregister_all(Uuid owner) = 0;

        /// @brief Whether any handler is registered for `method`.
        virtual bool has_method(std::string_view method) const = 0;
    };
}
