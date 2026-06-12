#pragma once
#include "tbx/systems/files/json.h"
#include <optional>
#include <string>
#include <string_view>

namespace tbx::rpc_communication
{
    constexpr int JSON_RPC_PARSE_ERROR_CODE = -32700;
    constexpr int JSON_RPC_METHOD_NOT_FOUND_CODE = -32601;
    constexpr int JSON_RPC_VIEW_UNAVAILABLE_CODE = -32000;
    constexpr int JSON_RPC_APPLY_FAILED_CODE = -32001;
    constexpr int JSON_RPC_INVALID_PARAMS_CODE = -32602;

    std::optional<Json> try_parse_message(const std::string& line);
    std::string make_result_response(const Json& id, const Json& result);
    std::string make_error_response(const Json& id, int code, std::string_view message);
    std::string make_notification(std::string_view method, const Json& params);
}
