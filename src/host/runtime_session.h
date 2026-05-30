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
    [[nodiscard]] bool submit_gameplay_action(const Gs1GameplayAction& action);
    [[nodiscard]] bool submit_site_move_direction(const Gs1SiteMoveDirectionCommand& command);
    [[nodiscard]] bool submit_site_action_request(const Gs1SiteActionRequestCommand& command);
    [[nodiscard]] bool submit_site_action_cancel(const Gs1SiteActionCancelCommand& command);
    [[nodiscard]] bool submit_site_context_request(const Gs1SiteContextRequestCommand& command);
    [[nodiscard]] bool submit_site_inventory_slot_tap(const Gs1SiteInventorySlotTapCommand& command);
    [[nodiscard]] bool submit_site_scene_ready();
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

