#include "rpc_registrar.h"
#include "tbx/systems/debugging/macros.h"
#include <utility>

namespace tbx::studio_bridge
{
    RpcRegistrar::RpcRegistrar(tbx::IRpcRouter& router, tbx::Uuid owner)
        : _router(router)
        , _owner(owner)
    {
    }

    void RpcRegistrar::add(std::string_view method, tbx::RpcHandler handler) const
    {
        if (const tbx::Result registered = _router.register_method(method, std::move(handler), _owner);
            !registered)
            TBX_TRACE_ERROR(
                "StudioBridge: could not register RPC method '{}': {}",
                method,
                registered.get_report());
    }

    void RpcRegistrar::add_query(std::string_view method, RpcQuery query) const
    {
        add(
            method,
            [query = std::move(query)](const tbx::Json& params, tbx::RpcResponder& responder)
            {
                auto reply = tbx::Json::object();
                const auto result = query(params, reply);
                responder.respond(result, reply);
            });
    }
}
