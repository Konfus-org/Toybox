#include "rpc_server.h"
#include "rpc_protocol.h"
#include <asio/buffer.hpp>
#include <asio/post.hpp>
#include <asio/read_until.hpp>
#include <asio/write.hpp>
#include <istream>
#include <utility>

namespace tbx::tcp_rpc
{
    RpcServer::~RpcServer() noexcept
    {
        stop();
    }

    Result RpcServer::start(uint16 port)
    {
        _io.restart();

        const auto endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port);
        auto& acceptor = _acceptor.emplace(_io);

        auto error = asio::error_code();
        acceptor.open(endpoint.protocol(), error);
        if (error)
        {
            _acceptor.reset();
            return Result(false, "Failed to create the listening socket.");
        }

        acceptor.bind(endpoint, error);
        if (error)
        {
            _acceptor.reset();
            return Result(false, "Failed to bind the listening socket; is the port in use?");
        }

        acceptor.listen(1, error);
        if (error)
        {
            _acceptor.reset();
            return Result(false, "Failed to listen on the bound socket.");
        }

        _port = port;
        _is_running = true;
        begin_accept();
        _work_guard.emplace(asio::make_work_guard(_io));
        _io_thread = std::thread([this]() { _io.run(); });
        return Result::OK;
    }

    void RpcServer::stop()
    {
        _is_running = false;
        _work_guard.reset();
        _io.stop();
        if (_io_thread.joinable())
            _io_thread.join();

        // The IO thread is gone, so the sockets can be torn down directly.
        _client.reset();
        _acceptor.reset();
        _send_queue.clear();
        _is_sending = false;
        _has_client = false;
    }

    bool RpcServer::has_client() const
    {
        return _has_client;
    }

    void RpcServer::send_notification(std::string_view method, const tbx::Json& params)
    {
        send_line(make_notification(method, params));
    }

    uint16 RpcServer::port() const
    {
        return _port;
    }

    std::vector<std::string> RpcServer::take_received_lines()
    {
        auto lock = std::lock_guard(_received_mutex);
        auto lines = std::vector<std::string>();
        lines.swap(_received_lines);
        return lines;
    }

    void RpcServer::send_line(const std::string& line)
    {
        asio::post(
            _io,
            [this, payload = line + "\n"]() mutable
            {
                if (!_client)
                    return;

                _send_queue.push_back(std::move(payload));
                if (!_is_sending)
                    begin_write();
            });
    }

    void RpcServer::begin_accept()
    {
        _acceptor->async_accept(
            [this](const asio::error_code& error, asio::ip::tcp::socket socket)
            {
                if (!_is_running)
                    return;

                if (error)
                {
                    begin_accept();
                    return;
                }

                _client.emplace(std::move(socket));
                _read_buffer.consume(_read_buffer.size());
                _send_queue.clear();
                _is_sending = false;
                _has_client = true;
                begin_read();
            });
    }

    void RpcServer::begin_read()
    {
        asio::async_read_until(
            *_client,
            _read_buffer,
            '\n',
            [this](const asio::error_code& error, size)
            {
                // The peer's failure handler may have torn the client down while this completion
                // was already queued; a stale completion must not touch the (cleared) state.
                if (!_is_running || !_client)
                    return;

                if (error)
                {
                    handle_client_closed();
                    return;
                }

                auto stream = std::istream(&_read_buffer);
                auto line = std::string();
                std::getline(stream, line);
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();

                if (!line.empty())
                {
                    auto lock = std::lock_guard(_received_mutex);
                    _received_lines.push_back(std::move(line));
                }

                begin_read();
            });
    }

    void RpcServer::begin_write()
    {
        _is_sending = true;
        asio::async_write(
            *_client,
            asio::buffer(_send_queue.front()),
            [this](const asio::error_code& error, size)
            {
                if (!_is_running || !_client)
                    return;

                if (error)
                {
                    handle_client_closed();
                    return;
                }

                _send_queue.pop_front();
                if (!_send_queue.empty())
                    begin_write();
                else
                    _is_sending = false;
            });
    }

    void RpcServer::handle_client_closed()
    {
        if (!_client)
            return;

        _client.reset();
        _send_queue.clear();
        _is_sending = false;
        _has_client = false;
        begin_accept();
    }
}
