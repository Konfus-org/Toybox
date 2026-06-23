#pragma once
#include "tbx/interfaces/rpc_host.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <atomic>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace tbx::windows_rpc
{
    constexpr uint64 INVALID_SOCKET_HANDLE = static_cast<uint64>(-1);

    /// @brief
    /// Purpose: Hosts a loopback TCP server that exchanges newline-delimited messages with a single
    /// connected client, and is the RPC transport (tbx::IRpcHost) consumer plugins push notifications
    /// through.
    /// @details
    /// Ownership: Owns the listening/client sockets and the IO thread. Thread Safety: send_line,
    /// take_received_lines, has_client, and send_notification are safe to call concurrently; start and
    /// stop must be called from the main thread.
    class RpcServer final : public tbx::IRpcHost
    {
      public:
        RpcServer() = default;
        ~RpcServer() noexcept override;

      public:
        RpcServer(const RpcServer&) = delete;
        RpcServer& operator=(const RpcServer&) = delete;
        RpcServer(RpcServer&&) = delete;
        RpcServer& operator=(RpcServer&&) = delete;

      public:
        Result start(uint16 port);
        void stop();
        std::vector<std::string> take_received_lines();
        void send_line(const std::string& line);

        // tbx::IRpcHost
        bool has_client() const override;
        void send_notification(std::string_view method, const tbx::Json& params) override;
        uint16 port() const override;

      private:
        void run_io_loop();
        void accept_pending_client();
        void receive_client_data();
        void close_client();

      private:
        uint16 _port = 0U;
        uint64 _listen_socket = INVALID_SOCKET_HANDLE;
        uint64 _client_socket = INVALID_SOCKET_HANDLE;
        std::atomic<bool> _is_running = false;
        std::atomic<bool> _has_client = false;
        std::thread _io_thread = {};
        std::mutex _client_mutex = {};
        std::mutex _received_mutex = {};
        std::vector<std::string> _received_lines = {};
        std::string _receive_buffer = {};
        bool _is_wsa_initialized = false;
    };
}
