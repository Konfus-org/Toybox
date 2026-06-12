#pragma once
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tbx::rpc_communication
{
    constexpr uint64 INVALID_SOCKET_HANDLE = static_cast<uint64>(-1);

    /// @brief
    /// Purpose: Hosts a loopback TCP server that exchanges newline-delimited messages with a
    /// single connected editor client.
    /// @details
    /// Ownership: Owns the listening/client sockets and the IO thread. Thread Safety: send_line,
    /// take_received_lines, and has_client are safe to call concurrently; start and stop must be
    /// called from the main thread.
    class RpcServer
    {
      public:
        RpcServer() = default;
        ~RpcServer() noexcept;

      public:
        RpcServer(const RpcServer&) = delete;
        RpcServer& operator=(const RpcServer&) = delete;
        RpcServer(RpcServer&&) = delete;
        RpcServer& operator=(RpcServer&&) = delete;

      public:
        Result start(uint16 port);
        void stop();
        bool has_client() const;
        std::vector<std::string> take_received_lines();
        void send_line(const std::string& line);

      private:
        void run_io_loop();
        void accept_pending_client();
        void receive_client_data();
        void close_client();

      private:
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
