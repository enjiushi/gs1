#include "gs1_godot_debug_http_server.h"

#include <array>
#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#endif

namespace
{
#if defined(_WIN32)
constexpr std::uintptr_t k_invalid_socket = static_cast<std::uintptr_t>(INVALID_SOCKET);

bool read_request(SOCKET client_socket, std::string& out_request)
{
    out_request.clear();
    std::array<char, 4096> buffer {};
    std::size_t header_end = std::string::npos;
    std::size_t content_length = 0U;

    while (true)
    {
        const int received = recv(client_socket, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (received <= 0)
        {
            return false;
        }

        out_request.append(buffer.data(), static_cast<std::size_t>(received));

        if (header_end == std::string::npos)
        {
            header_end = out_request.find("\r\n\r\n");
            if (header_end != std::string::npos)
            {
                const auto content_length_pos = out_request.find("Content-Length:");
                if (content_length_pos != std::string::npos)
                {
                    const auto value_start = content_length_pos + std::string_view("Content-Length:").size();
                    const auto value_end = out_request.find("\r\n", value_start);
                    const auto raw_value = out_request.substr(value_start, value_end - value_start);
                    content_length = static_cast<std::size_t>(std::strtoul(raw_value.c_str(), nullptr, 10));
                }
            }
        }

        if (header_end != std::string::npos)
        {
            const std::size_t body_size = out_request.size() - (header_end + 4U);
            if (body_size >= content_length)
            {
                return true;
            }
        }
    }
}
#endif
}

Gs1GodotDebugHttpServer::~Gs1GodotDebugHttpServer() noexcept
{
    stop();
}

bool Gs1GodotDebugHttpServer::start(std::uint16_t preferred_port, CommandCallback command_callback, std::string& out_error)
{
    if (running_.load())
    {
        return true;
    }

    command_callback_ = std::move(command_callback);
    port_ = 0;
    listen_socket_ = 0;

#if !defined(_WIN32)
    out_error = "Godot debug HTTP control is only implemented for Windows builds.";
    command_callback_ = {};
    return false;
#else
    WSADATA wsa_data {};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        out_error = "WSAStartup failed.";
        command_callback_ = {};
        return false;
    }

    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET)
    {
        out_error = "socket() failed.";
        WSACleanup();
        command_callback_ = {};
        return false;
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(preferred_port);

    if (bind(listen_socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
    {
        out_error = "bind() failed.";
        closesocket(listen_socket);
        WSACleanup();
        command_callback_ = {};
        return false;
    }

    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        out_error = "listen() failed.";
        closesocket(listen_socket);
        WSACleanup();
        command_callback_ = {};
        return false;
    }

    sockaddr_in bound_address {};
    int bound_address_size = sizeof(bound_address);
    if (getsockname(listen_socket, reinterpret_cast<sockaddr*>(&bound_address), &bound_address_size) != 0)
    {
        out_error = "getsockname() failed.";
        closesocket(listen_socket);
        WSACleanup();
        command_callback_ = {};
        return false;
    }

    port_ = ntohs(bound_address.sin_port);
    listen_socket_ = static_cast<std::uintptr_t>(listen_socket);
    running_.store(true);
    server_thread_ = std::thread(&Gs1GodotDebugHttpServer::server_loop, this);
    return true;
#endif
}

void Gs1GodotDebugHttpServer::stop() noexcept
{
    if (!running_.load())
    {
        return;
    }

    running_.store(false);

#if defined(_WIN32)
    if (listen_socket_ != k_invalid_socket)
    {
        closesocket(static_cast<SOCKET>(listen_socket_));
        listen_socket_ = k_invalid_socket;
    }
#endif

    if (server_thread_.joinable())
    {
        server_thread_.join();
    }

#if defined(_WIN32)
    WSACleanup();
#endif

    command_callback_ = {};
    port_ = 0;
}

void Gs1GodotDebugHttpServer::server_loop() noexcept
{
#if defined(_WIN32)
    const SOCKET listen_socket = static_cast<SOCKET>(listen_socket_);

    while (running_.load())
    {
        fd_set sockets {};
        FD_ZERO(&sockets);
        FD_SET(listen_socket, &sockets);

        timeval timeout {};
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;

        const int ready = select(0, &sockets, nullptr, nullptr, &timeout);
        if (ready <= 0)
        {
            continue;
        }

        const SOCKET client_socket = accept(listen_socket, nullptr, nullptr);
        if (client_socket == INVALID_SOCKET)
        {
            continue;
        }

        handle_client(static_cast<std::uintptr_t>(client_socket));
        closesocket(client_socket);
    }
#endif
}

void Gs1GodotDebugHttpServer::handle_client(std::uintptr_t client_socket_value)
{
#if defined(_WIN32)
    const SOCKET client_socket = static_cast<SOCKET>(client_socket_value);
    std::string request {};
    if (!read_request(client_socket, request))
    {
        return;
    }

    const std::size_t first_line_end = request.find("\r\n");
    if (first_line_end == std::string::npos)
    {
        send_response(client_socket_value, 400, "Bad Request", "text/plain; charset=utf-8", "Malformed request.");
        return;
    }

    std::istringstream first_line_stream {request.substr(0, first_line_end)};
    std::string method {};
    std::string path {};
    std::string version {};
    first_line_stream >> method >> path >> version;
    (void)version;

    const std::size_t header_end = request.find("\r\n\r\n");
    const std::string body = header_end == std::string::npos ? std::string {} : request.substr(header_end + 4U);

    if (method == "GET" && path == "/health")
    {
        send_response(client_socket_value, 200, "OK", "application/json; charset=utf-8", "{\"ok\":true}");
        return;
    }

    if (method != "POST")
    {
        send_response(client_socket_value, 405, "Method Not Allowed", "text/plain; charset=utf-8", "Use POST.");
        return;
    }

    if (path != "/gameplay-action" &&
        path != "/site-action" &&
        path != "/site-action-cancel" &&
        path != "/site-storage-view" &&
        path != "/site-inventory-slot-tap" &&
        path != "/site-context" &&
        path != "/site-control")
    {
        send_response(client_socket_value, 404, "Not Found", "text/plain; charset=utf-8", "Unknown endpoint.");
        return;
    }

    std::string error {};
    if (command_callback_ == nullptr)
    {
        send_response(client_socket_value, 503, "Service Unavailable", "text/plain; charset=utf-8", "No command callback.");
        return;
    }

    if (!command_callback_(path, body, error))
    {
        if (error.empty())
        {
            error = "Command rejected.";
        }
        send_response(client_socket_value, 400, "Bad Request", "text/plain; charset=utf-8", error);
        return;
    }

    send_response(client_socket_value, 200, "OK", "application/json; charset=utf-8", "{\"ok\":true}");
#else
    (void)client_socket_value;
#endif
}

bool Gs1GodotDebugHttpServer::send_all(std::uintptr_t client_socket_value, std::string_view payload)
{
#if !defined(_WIN32)
    (void)client_socket_value;
    (void)payload;
    return false;
#else
    const SOCKET client_socket = static_cast<SOCKET>(client_socket_value);
    const char* data = payload.data();
    int remaining = static_cast<int>(payload.size());
    while (remaining > 0)
    {
        const int sent = send(client_socket, data, remaining, 0);
        if (sent <= 0)
        {
            return false;
        }

        data += sent;
        remaining -= sent;
    }

    return true;
#endif
}

void Gs1GodotDebugHttpServer::send_response(
    std::uintptr_t client_socket_value,
    int status_code,
    const char* status_text,
    const char* content_type,
    std::string_view body)
{
    std::ostringstream response {};
    response << "HTTP/1.1 " << status_code << ' ' << status_text << "\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Cache-Control: no-store\r\n";
    response << "Connection: close\r\n";
    response << "Content-Length: " << body.size() << "\r\n\r\n";
    response << body;

    (void)send_all(client_socket_value, response.str());
}
