#include "rpc_protocol.h"

namespace tbx::tcp_rpc
{
    std::optional<Json> try_parse_message(const std::string& line)
    {
        auto json = Json::parse(line, nullptr, false);
        if (json.is_discarded() || !json.is_object())
            return std::nullopt;

        return json;
    }

    std::string make_result_response(const Json& id, const Json& result)
    {
        auto response = Json::object();
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["result"] = result;
        return response.dump();
    }

    std::string make_error_response(const Json& id, int code, std::string_view message)
    {
        auto error = Json::object();
        error["code"] = code;
        error["message"] = std::string(message);

        auto response = Json::object();
        response["jsonrpc"] = "2.0";
        response["id"] = id;
        response["error"] = error;
        return response.dump();
    }

    std::string make_notification(std::string_view method, const Json& params)
    {
        auto notification = Json::object();
        notification["jsonrpc"] = "2.0";
        notification["method"] = std::string(method);
        notification["params"] = params;
        return notification.dump();
    }
}
