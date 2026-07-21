#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <string_view>

namespace tbx
{
    /// @brief
    /// Purpose: The RPC transport a consumer plugin pushes through — unsolicited notifications to the
    /// connected client plus connection/port queries. Replies to a request go through RpcResponder
    /// instead; this interface is for engine->client pushes (log lines, view-surface events, …).
    /// @details
    /// Ownership: Published as a service by the transport plugin (TcpRpc); consumers hold a
    /// weak_ptr and lock at the point of use. Thread Safety: Main-thread only.
    class TBX_API IRpcHost
    {
      public:
        IRpcHost();
        virtual ~IRpcHost() noexcept;

      public:
        /// @brief Whether a client is currently connected. A notification sent with no client is dropped.
        virtual bool has_client() const = 0;

        /// @brief Pushes a JSON-RPC notification (no id, no reply) carrying `method` + `params` to the client.
        virtual void send_notification(std::string_view method, const Json& params) = 0;

        /// @brief The TCP port the host is listening on (0 until started).
        virtual uint16 port() const = 0;
    };
}
