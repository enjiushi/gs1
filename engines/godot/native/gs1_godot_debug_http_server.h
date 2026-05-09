#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <thread>

class Gs1GodotDebugHttpServer final
{
public:
    using CommandCallback = std::function<bool(std::string_view path, const std::string& body, std::string& out_error)>;

    Gs1GodotDebugHttpServer() = default;
    Gs1GodotDebugHttpServer(const Gs1GodotDebugHttpServer&) = delete;
    Gs1GodotDebugHttpServer& operator=(const Gs1GodotDebugHttpServer&) = delete;
    ~Gs1GodotDebugHttpServer() noexcept;

    [[nodiscard]] bool start(std::uint16_t preferred_port, CommandCallback command_callback, std::string& out_error);
    void stop() noexcept;

    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }
    [[nodiscard]] bool is_running() const noexcept { return running_.load(); }

private:
    void server_loop() noexcept;
    void handle_client(std::uintptr_t client_socket_value);
    static bool send_all(std::uintptr_t client_socket_value, std::string_view payload);
    static void send_response(
        std::uintptr_t client_socket_value,
        int status_code,
        const char* status_text,
        const char* content_type,
        std::string_view body);

private:
    CommandCallback command_callback_ {};
    std::atomic<bool> running_ {false};
    std::uintptr_t listen_socket_ {0};
    std::thread server_thread_ {};
    std::uint16_t port_ {0};
};
