#include "smoke_live_http_server.h"

#include <array>
#include <cstdlib>
#include <fstream>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

namespace
{
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
            const auto body_size = out_request.size() - (header_end + 4U);
            if (body_size >= content_length)
            {
                return true;
            }
        }
    }
}

}  // namespace

SmokeLiveHttpServer::SmokeLiveHttpServer(
    std::filesystem::path repo_root,
    GetStringCallback state_callback,
    PostBodyCallback ui_action_callback,
    PostBodyCallback site_action_callback,
    PostBodyCallback site_action_cancel_callback,
    PostBodyCallback site_storage_view_callback,
    PostBodyCallback site_context_callback,
    PostBodyCallback site_control_callback,
    PostBodyCallback client_log_callback)
    : repo_root_(std::move(repo_root))
    , state_callback_(std::move(state_callback))
    , ui_action_callback_(std::move(ui_action_callback))
    , site_action_callback_(std::move(site_action_callback))
    , site_action_cancel_callback_(std::move(site_action_cancel_callback))
    , site_storage_view_callback_(std::move(site_storage_view_callback))
    , site_context_callback_(std::move(site_context_callback))
    , site_control_callback_(std::move(site_control_callback))
    , client_log_callback_(std::move(client_log_callback))
{
}

SmokeLiveHttpServer::~SmokeLiveHttpServer() noexcept
{
    stop();
}

bool SmokeLiveHttpServer::start(std::uint16_t preferred_port)
{
    if (running_)
    {
        return true;
    }

    WSADATA wsa_data {};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        return false;
    }

    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET)
    {
        WSACleanup();
        return false;
    }

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(preferred_port);

    if (bind(listen_socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
    {
        closesocket(listen_socket);
        WSACleanup();
        return false;
    }

    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(listen_socket);
        WSACleanup();
        return false;
    }

    sockaddr_in bound_address {};
    int bound_address_size = sizeof(bound_address);
    if (getsockname(listen_socket, reinterpret_cast<sockaddr*>(&bound_address), &bound_address_size) != 0)
    {
        closesocket(listen_socket);
        WSACleanup();
        return false;
    }

    port_ = ntohs(bound_address.sin_port);
    listen_socket_ = static_cast<std::uintptr_t>(listen_socket);
    running_ = true;
    server_thread_ = std::thread(&SmokeLiveHttpServer::server_loop, this);
    return true;
}

void SmokeLiveHttpServer::stop() noexcept
{
    if (!running_)
    {
        return;
    }

    running_ = false;

    if (listen_socket_ != k_invalid_socket)
    {
        closesocket(static_cast<SOCKET>(listen_socket_));
        listen_socket_ = k_invalid_socket;
    }

    {
        std::scoped_lock lock {event_clients_mutex_};
        for (const auto client_socket : event_client_sockets_)
        {
            closesocket(static_cast<SOCKET>(client_socket));
        }
        event_client_sockets_.clear();
    }

    if (server_thread_.joinable())
    {
        server_thread_.join();
    }

    WSACleanup();
}

void SmokeLiveHttpServer::server_loop() noexcept
{
    const SOCKET listen_socket = static_cast<SOCKET>(listen_socket_);

    while (running_)
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

        const bool keep_open = handle_client(static_cast<std::uintptr_t>(client_socket));
        if (!keep_open)
        {
            closesocket(client_socket);
        }
    }
}

bool SmokeLiveHttpServer::handle_client(std::uintptr_t client_socket_value)
{
    const SOCKET client_socket = static_cast<SOCKET>(client_socket_value);
    std::string request {};
    if (!read_request(client_socket, request))
    {
        return false;
    }

    const auto first_line_end = request.find("\r\n");
    if (first_line_end == std::string::npos)
    {
        send_response(client_socket, 400, "Bad Request", "text/plain; charset=utf-8", "Malformed request.");
        return false;
    }

    std::istringstream first_line_stream {request.substr(0, first_line_end)};
    std::string method {};
    std::string path {};
    std::string version {};
    first_line_stream >> method >> path >> version;
    (void)version;

    const auto header_end = request.find("\r\n\r\n");
    const std::string body = header_end == std::string::npos ? std::string {} : request.substr(header_end + 4U);

    if (method == "GET" && (path == "/" || path == "/index.html"))
    {
        const auto html = load_text_file(repo_root_ / "tests" / "smoke" / "visual_smoke_live.html");
        if (html.empty())
        {
            send_response(client_socket, 404, "Not Found", "text/plain; charset=utf-8", "Missing live viewer HTML.");
        }
        else
        {
            send_response(client_socket, 200, "OK", "text/html; charset=utf-8", html);
        }
        return false;
    }

    if (method == "GET" && path == "/viewer.js")
    {
        const auto script = load_text_file(repo_root_ / "tests" / "smoke" / "visual_smoke_live_viewer.js");
        if (script.empty())
        {
            send_response(client_socket, 404, "Not Found", "text/plain; charset=utf-8", "Missing live viewer script.");
        }
        else
        {
            send_response(client_socket, 200, "OK", "application/javascript; charset=utf-8", script);
        }
        return false;
    }

    if (method == "GET" && path == "/content/technology_nodes.toml")
    {
        const auto toml_text = load_text_file(repo_root_ / "project" / "content" / "technology_nodes.toml");
        if (toml_text.empty())
        {
            send_response(client_socket, 404, "Not Found", "text/plain; charset=utf-8", "Missing technology node table.");
        }
        else
        {
            send_response(client_socket, 200, "OK", "text/plain; charset=utf-8", toml_text);
        }
        return false;
    }

    if (method == "GET" && path == "/content/technology_tiers.toml")
    {
        const auto toml_text = load_text_file(repo_root_ / "project" / "content" / "technology_tiers.toml");
        if (toml_text.empty())
        {
            send_response(client_socket, 404, "Not Found", "text/plain; charset=utf-8", "Missing technology tier table.");
        }
        else
        {
            send_response(client_socket, 200, "OK", "text/plain; charset=utf-8", toml_text);
        }
        return false;
    }

    if (method == "GET" && path == "/content/reputation_unlocks.toml")
    {
        const auto toml_text = load_text_file(repo_root_ / "project" / "content" / "reputation_unlocks.toml");
        if (toml_text.empty())
        {
            send_response(client_socket, 404, "Not Found", "text/plain; charset=utf-8", "Missing reputation unlock table.");
        }
        else
        {
            send_response(client_socket, 200, "OK", "text/plain; charset=utf-8", toml_text);
        }
        return false;
    }

    if (method == "GET" && path == "/assets/main-menu-desert.png")
    {
        const auto image = load_text_file(repo_root_ / "tests" / "smoke" / "assets" / "main-menu-desert.png");
        if (image.empty())
        {
            send_response(client_socket, 404, "Not Found", "text/plain; charset=utf-8", "Missing main menu image.");
        }
        else
        {
            send_response(client_socket, 200, "OK", "image/png", image);
        }
        return false;
    }

    if (method == "GET" && path == "/state")
    {
        send_response(client_socket, 200, "OK", "application/json; charset=utf-8", state_callback_());
        return false;
    }

    if (method == "GET" && path == "/events")
    {
        std::scoped_lock lock {event_clients_mutex_};
        if (!send_sse_headers(client_socket_value) ||
            !send_sse_event(client_socket_value, "full-state", state_callback_()))
        {
            return false;
        }

        event_client_sockets_.push_back(client_socket_value);
        return true;
    }

    if (method == "POST" && path == "/ui-action")
    {
        ui_action_callback_(body);
        send_response(client_socket, 200, "OK", "application/json; charset=utf-8", "{\"ok\":true}");
        return false;
    }

    if (method == "POST" && path == "/site-action")
    {
        site_action_callback_(body);
        send_response(client_socket, 200, "OK", "application/json; charset=utf-8", "{\"ok\":true}");
        return false;
    }

    if (method == "POST" && path == "/site-action-cancel")
    {
        site_action_cancel_callback_(body);
        send_response(client_socket, 200, "OK", "application/json; charset=utf-8", "{\"ok\":true}");
        return false;
    }

    if (method == "POST" && path == "/site-storage-view")
    {
        site_storage_view_callback_(body);
        send_response(client_socket, 200, "OK", "application/json; charset=utf-8", "{\"ok\":true}");
        return false;
    }

    if (method == "POST" && path == "/site-context")
    {
        site_context_callback_(body);
        send_response(client_socket, 200, "OK", "application/json; charset=utf-8", "{\"ok\":true}");
        return false;
    }

    if (method == "POST" && path == "/site-control")
    {
        site_control_callback_(body);
        send_response(client_socket, 200, "OK", "application/json; charset=utf-8", "{\"ok\":true}");
        return false;
    }

    if (method == "POST" && path == "/client-log")
    {
        client_log_callback_(body);
        send_response(client_socket, 200, "OK", "application/json; charset=utf-8", "{\"ok\":true}");
        return false;
    }

    if (method == "GET" && path == "/health")
    {
        send_response(client_socket, 200, "OK", "application/json; charset=utf-8", "{\"ok\":true}");
        return false;
    }

    send_response(client_socket, 404, "Not Found", "text/plain; charset=utf-8", "Unknown endpoint.");
    return false;
}

void SmokeLiveHttpServer::publish_event(std::string_view event_name, std::string_view data)
{
    std::scoped_lock lock {event_clients_mutex_};

    for (auto it = event_client_sockets_.begin(); it != event_client_sockets_.end();)
    {
        if (!send_sse_event(*it, event_name, data))
        {
            closesocket(static_cast<SOCKET>(*it));
            it = event_client_sockets_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

std::string SmokeLiveHttpServer::load_text_file(const std::filesystem::path& path) const
{
    std::ifstream input {path, std::ios::binary};
    if (!input.is_open())
    {
        return {};
    }

    std::ostringstream stream {};
    stream << input.rdbuf();
    return stream.str();
}

bool SmokeLiveHttpServer::send_all(std::uintptr_t client_socket_value, std::string_view payload)
{
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
}

bool SmokeLiveHttpServer::send_sse_headers(std::uintptr_t client_socket_value)
{
    std::ostringstream response {};
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/event-stream\r\n";
    response << "Cache-Control: no-store\r\n";
    response << "Connection: keep-alive\r\n";
    response << "X-Accel-Buffering: no\r\n\r\n";
    return send_all(client_socket_value, response.str());
}

bool SmokeLiveHttpServer::send_sse_event(
    std::uintptr_t client_socket_value,
    std::string_view event_name,
    std::string_view data)
{
    std::ostringstream response {};
    response << "event: " << event_name << "\n";

    std::size_t line_start = 0U;
    while (line_start <= data.size())
    {
        const auto line_end = data.find('\n', line_start);
        response << "data: ";
        if (line_end == std::string_view::npos)
        {
            response << data.substr(line_start) << "\n\n";
            break;
        }

        response << data.substr(line_start, line_end - line_start) << "\n";
        line_start = line_end + 1U;
    }

    return send_all(client_socket_value, response.str());
}

void SmokeLiveHttpServer::send_response(
    std::uintptr_t client_socket_value,
    int status_code,
    const char* status_text,
    const char* content_type,
    const std::string& body)
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
