#pragma once
#include "tbx/interfaces/rpc_host.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/streambuf.hpp>
#include <atomic>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace tbx::tcp_rpc
{
    /// @brief
    /// Purpose: Hosts a loopback TCP server that exchanges newline-delimited messages with a single
    /// connected client, and is the RPC transport (tbx::IRpcHost) consumer plugins push notifications
    /// through.
    /// @details
    /// Ownership: Owns the io_context, the sockets, and the IO thread that runs them. Thread Safety:
    /// send_line, take_received_lines, has_client, and send_notification are safe to call
    /// concurrently (all socket work is posted to the IO thread); start and stop must be called from
    /// the main thread.
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
        // The private methods below run exclusively on the IO thread.
        void begin_accept();
        void begin_read();
        void begin_write();
        void handle_client_closed();

      private:
        uint16 _port = 0U;
        std::atomic<bool> _is_running = false;
        std::atomic<bool> _has_client = false;
        asio::io_context _io = {};
        std::optional<asio::executor_work_guard<asio::io_context::executor_type>> _work_guard = {};
        std::optional<asio::ip::tcp::acceptor> _acceptor = {};
        std::optional<asio::ip::tcp::socket> _client = {};
        // Direct-init: streambuf's constructor is explicit, so `= {}` is ill-formed.
        asio::streambuf _read_buffer {};
        std::deque<std::string> _send_queue = {};
        bool _is_sending = false;
        std::thread _io_thread = {};
        std::mutex _received_mutex = {};
        std::vector<std::string> _received_lines = {};
    };
}
