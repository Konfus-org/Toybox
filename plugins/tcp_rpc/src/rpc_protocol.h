#pragma once
#include "tbx/systems/files/json.h"
#include <optional>
#include <string>
#include <string_view>

namespace tbx::tcp_rpc
{
    // JSON-RPC wire formatting. Standard error codes live in tbx/interfaces/rpc_router.h
    // (RPC_PARSE_ERROR_CODE etc.) so transport and consumers share them.
    std::optional<Json> try_parse_message(const std::string& line);
    std::string make_result_response(const Json& id, const Json& result);
    std::string make_error_response(const Json& id, int code, std::string_view message);
    std::string make_notification(std::string_view method, const Json& params);
}
