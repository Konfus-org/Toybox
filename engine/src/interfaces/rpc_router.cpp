#include "tbx/interfaces/rpc_router.h"

namespace tbx
{
    RpcResponder::RpcResponder() = default;

    RpcResponder::~RpcResponder() noexcept = default;

    void RpcResponder::respond(const Result& outcome, const Json& reply)
    {
        if (outcome)
            result(reply);
        else
            error(RPC_APPLY_FAILED_CODE, outcome.get_report());
    }

    IRpcRouter::IRpcRouter() = default;

    IRpcRouter::~IRpcRouter() noexcept = default;
}
