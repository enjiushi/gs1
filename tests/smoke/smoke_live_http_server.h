#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>

class SmokeLiveHttpServer final
{
public:
    using GetStringCallback = std::function<std::string()>;
    using PostBodyCallback = std::function<void(const std::string&)>;

    SmokeLiveHttpServer(
        std::filesystem::path repo_root,
        GetStringCallback state_callback,
        PostBodyCallback ui_action_callback,
        PostBodyCallback input_callback);
    SmokeLiveHttpServer(const SmokeLiveHttpServer&) = delete;
    SmokeLiveHttpServer& operator=(const SmokeLiveHttpServer&) = delete;
    ~SmokeLiveHttpServer() noexcept;

    [[nodiscard]] bool start(std::uint16_t preferred_port = 0U);
    void stop() noexcept;

    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

private:
    void server_loop() noexcept;
    void handle_client(std::uintptr_t client_socket);
    [[nodiscard]] std::string load_text_file(const std::filesystem::path& path) const;
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
    PostBodyCallback input_callback_ {};
    std::atomic<bool> running_ {false};
    std::uintptr_t listen_socket_ {static_cast<std::uintptr_t>(~0ULL)};
    std::thread server_thread_ {};
    std::uint16_t port_ {0};
};
