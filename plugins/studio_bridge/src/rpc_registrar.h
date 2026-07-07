#pragma once
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/uuid.h"
#include "tbx/utils/result.h"
#include <functional>
#include <string_view>

namespace tbx::studio_bridge
{
    // A query-style RPC implementation: fills `out_reply` and returns the outcome; the registrar
    // supplies the reply object and the respond plumbing.
    using RpcQuery = std::function<tbx::Result(const tbx::Json& params, tbx::Json& out_reply)>;

    /// @brief
    /// Purpose: Registers a group of RPC methods on the router under one owner, logging any collision
    /// (a method another plugin already owns is kept by the router and ours dropped, silently breaking
    /// that part of the editor protocol — so surface it). Handed to each `register_*_handlers` free
    /// function so the category files share the lookup + error plumbing instead of repeating it.
    /// @details
    /// Ownership: Borrows the router; stack-scoped for the duration of registration. Thread Safety:
    /// Main-thread only (registration happens during on_attach).
    class RpcRegistrar
    {
      public:
        RpcRegistrar(tbx::IRpcRouter& router, tbx::Uuid owner);

      public:
        // Registers `handler` for `method`; logs an error (and drops the handler) on an ownership
        // collision. Const so the category functions can take the registrar by const reference.
        void add(std::string_view method, tbx::RpcHandler handler) const;

        // Registers a query for `method`, wrapping it with the make-reply/run/respond plumbing the
        // reply-carrying methods would otherwise each repeat.
        void add_query(std::string_view method, RpcQuery query) const;

      private:
        tbx::IRpcRouter& _router;
        tbx::Uuid _owner;
    };
}
