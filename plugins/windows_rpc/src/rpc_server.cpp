#include "rpc_server.h"
#include "rpc_protocol.h"
#include <winsock2.h>
#include <ws2tcpip.h>

namespace tbx::windows_rpc
{
    static constexpr int IO_POLL_TIMEOUT_MILLISECONDS = 100;

    static bool wait_for_readable(uint64 socket_handle)
    {
        auto socket_value = static_cast<SOCKET>(socket_handle);
        auto read_set = fd_set();
        FD_ZERO(&read_set);
        FD_SET(socket_value, &read_set);

        auto timeout = timeval();
        timeout.tv_sec = 0;
        timeout.tv_usec = IO_POLL_TIMEOUT_MILLISECONDS * 1000;

        return ::select(0, &read_set, nullptr, nullptr, &timeout) == 1;
    }

    RpcServer::~RpcServer() noexcept
    {
        stop();
    }

    Result RpcServer::start(uint16 port)
    {
        auto wsa_data = WSADATA();
        if (::WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
            return Result(false, "WSAStartup failed.");
        _is_wsa_initialized = true;

        auto listen_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_socket == INVALID_SOCKET)
            return Result(false, "Failed to create the listening socket.");

        auto address = sockaddr_in();
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
        address.sin_port = ::htons(port);

        if (::bind(listen_socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address))
            == SOCKET_ERROR)
        {
            ::closesocket(listen_socket);
            return Result(false, "Failed to bind the listening socket; is the port in use?");
        }

        if (::listen(listen_socket, 1) == SOCKET_ERROR)
        {
            ::closesocket(listen_socket);
            return Result(false, "Failed to listen on the bound socket.");
        }

        _port = port;
        _listen_socket = static_cast<uint64>(listen_socket);
        _is_running = true;
        _io_thread = std::thread([this]() { run_io_loop(); });
        return Result::OK;
    }

    void RpcServer::stop()
    {
        _is_running = false;
        if (_io_thread.joinable())
            _io_thread.join();

        close_client();
        if (_listen_socket != INVALID_SOCKET_HANDLE)
        {
            ::closesocket(static_cast<SOCKET>(_listen_socket));
            _listen_socket = INVALID_SOCKET_HANDLE;
        }

        if (_is_wsa_initialized)
        {
            ::WSACleanup();
            _is_wsa_initialized = false;
        }
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
        auto lock = std::lock_guard(_client_mutex);
        if (_client_socket == INVALID_SOCKET_HANDLE)
            return;

        auto payload = line + "\n";
        auto socket_value = static_cast<SOCKET>(_client_socket);
        auto sent_total = static_cast<size>(0);
        while (sent_total < payload.size())
        {
            const auto sent = ::send(
                socket_value,
                payload.data() + sent_total,
                static_cast<int>(payload.size() - sent_total),
                0);
            if (sent == SOCKET_ERROR)
            {
                ::closesocket(socket_value);
                _client_socket = INVALID_SOCKET_HANDLE;
                _has_client = false;
                return;
            }

            sent_total += static_cast<size>(sent);
        }
    }

    void RpcServer::run_io_loop()
    {
        while (_is_running)
        {
            if (!_has_client)
                accept_pending_client();
            else
                receive_client_data();
        }
    }

    void RpcServer::accept_pending_client()
    {
        if (!wait_for_readable(_listen_socket))
            return;

        const auto client_socket = ::accept(static_cast<SOCKET>(_listen_socket), nullptr, nullptr);
        if (client_socket == INVALID_SOCKET)
            return;

        {
            auto lock = std::lock_guard(_client_mutex);
            _client_socket = static_cast<uint64>(client_socket);
        }

        _receive_buffer.clear();
        _has_client = true;
    }

    void RpcServer::receive_client_data()
    {
        auto socket_handle = INVALID_SOCKET_HANDLE;
        {
            auto lock = std::lock_guard(_client_mutex);
            socket_handle = _client_socket;
        }

        if (socket_handle == INVALID_SOCKET_HANDLE)
        {
            _has_client = false;
            return;
        }

        if (!wait_for_readable(socket_handle))
            return;

        char buffer[4096];
        const auto received =
            ::recv(static_cast<SOCKET>(socket_handle), buffer, sizeof(buffer), 0);
        if (received <= 0)
        {
            close_client();
            return;
        }

        _receive_buffer.append(buffer, static_cast<size>(received));

        auto line_end = _receive_buffer.find('\n');
        while (line_end != std::string::npos)
        {
            auto line = _receive_buffer.substr(0, line_end);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (!line.empty())
            {
                auto lock = std::lock_guard(_received_mutex);
                _received_lines.push_back(std::move(line));
            }

            _receive_buffer.erase(0, line_end + 1);
            line_end = _receive_buffer.find('\n');
        }
    }

    void RpcServer::close_client()
    {
        auto lock = std::lock_guard(_client_mutex);
        if (_client_socket != INVALID_SOCKET_HANDLE)
        {
            ::closesocket(static_cast<SOCKET>(_client_socket));
            _client_socket = INVALID_SOCKET_HANDLE;
        }

        _has_client = false;
    }
}
