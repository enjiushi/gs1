#pragma once

#include "host/adapter_config_loader.h"
#include "host/runtime_dll_loader.h"

#include <filesystem>
#include <optional>
#include <string>

class Gs1RuntimeSession final
{
public:
    Gs1RuntimeSession() noexcept = default;
    Gs1RuntimeSession(const Gs1RuntimeSession&) = delete;
    Gs1RuntimeSession& operator=(const Gs1RuntimeSession&) = delete;
    ~Gs1RuntimeSession() noexcept;

    [[nodiscard]] bool start(
        const std::filesystem::path& gameplay_dll_path,
        const std::filesystem::path& project_config_root,
        const Gs1AdapterConfigBlob* adapter_config,
        double fixed_step_seconds = 1.0 / 60.0);
    void stop() noexcept;

    [[nodiscard]] bool is_running() const noexcept { return runtime_ != nullptr; }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }
    [[nodiscard]] const std::filesystem::path& gameplay_dll_path() const noexcept { return gameplay_dll_path_; }
    [[nodiscard]] const Gs1RuntimeApi* api() const noexcept
    {
        return runtime_ != nullptr ? &loader_.api() : nullptr;
    }
    [[nodiscard]] Gs1RuntimeHandle* runtime() const noexcept { return runtime_; }

    [[nodiscard]] bool run_phase1(double delta_seconds, Gs1Phase1Result& out_phase1);
    [[nodiscard]] bool run_phase2(Gs1Phase2Result& out_phase2);
    [[nodiscard]] bool update(double delta_seconds, Gs1Phase1Result& out_phase1, Gs1Phase2Result& out_phase2);
    [[nodiscard]] bool submit_host_messages(const Gs1HostMessage* messages, std::uint32_t message_count);
    [[nodiscard]] bool submit_host_events(const Gs1HostEvent* messages, std::uint32_t message_count);
    [[nodiscard]] bool pop_runtime_message(Gs1RuntimeMessage& out_message);
    [[nodiscard]] bool pop_engine_message(Gs1EngineMessage& out_message);
    [[nodiscard]] bool get_game_state_view(Gs1GameStateView& out_view);
    [[nodiscard]] bool query_site_tile_view(std::uint32_t tile_index, Gs1SiteTileView& out_tile);

private:
    RuntimeDllLoader loader_ {};
    Gs1RuntimeHandle* runtime_ {nullptr};
    std::filesystem::path gameplay_dll_path_ {};
    std::string project_config_root_utf8_ {};
    std::string adapter_config_json_utf8_ {};
    std::string last_error_ {};
};

