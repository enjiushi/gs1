#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

class SmokeLiveHttpServer final
{
public:
    using GetStringCallback = std::function<std::string()>;
    using PostBodyCallback = std::function<void(const std::string&)>;

    SmokeLiveHttpServer(
        std::filesystem::path repo_root,
        GetStringCallback state_callback,
        PostBodyCallback ui_action_callback,
        PostBodyCallback site_action_callback,
        PostBodyCallback site_action_cancel_callback,
        PostBodyCallback site_storage_view_callback,
        PostBodyCallback site_context_callback,
        PostBodyCallback site_control_callback,
        PostBodyCallback client_log_callback);
    SmokeLiveHttpServer(const SmokeLiveHttpServer&) = delete;
    SmokeLiveHttpServer& operator=(const SmokeLiveHttpServer&) = delete;
    ~SmokeLiveHttpServer() noexcept;

    [[nodiscard]] bool start(std::uint16_t preferred_port = 0U);
    void stop() noexcept;
    void publish_event(std::string_view event_name, std::string_view data);

    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

private:
    void server_loop() noexcept;
    [[nodiscard]] bool handle_client(std::uintptr_t client_socket);
    [[nodiscard]] std::string load_text_file(const std::filesystem::path& path) const;
    static bool send_all(std::uintptr_t client_socket, std::string_view payload);
    static bool send_sse_headers(std::uintptr_t client_socket);
    static bool send_sse_event(
        std::uintptr_t client_socket,
        std::string_view event_name,
        std::string_view data);
    static void send_response(
        std::uintptr_t client_socket,
        int status_code,
        const char* status_text,
        const char* content_type,
        const std::string& body);

private:
    std::filesystem::path repo_root_ {};
    GetStringCallback state_callback_ {};
    PostBodyCallback ui_action_callback_ {};
    PostBodyCallback site_action_callback_ {};
    PostBodyCallback site_action_cancel_callback_ {};
    PostBodyCallback site_storage_view_callback_ {};
    PostBodyCallback site_context_callback_ {};
    PostBodyCallback site_control_callback_ {};
    PostBodyCallback client_log_callback_ {};
    std::atomic<bool> running_ {false};
    std::uintptr_t listen_socket_ {static_cast<std::uintptr_t>(~0ULL)};
    std::thread server_thread_ {};
    std::mutex event_clients_mutex_ {};
    std::vector<std::uintptr_t> event_client_sockets_ {};
    std::uint16_t port_ {0};
};
