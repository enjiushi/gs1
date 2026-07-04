#pragma once

#include "gs1/game_api.h"

#include <string>

namespace shared_framework::host {
struct RuntimeApi final
{
    decltype(&gs1_get_api_version) get_api_version {nullptr};
    decltype(&gs1_get_build_string) get_build_string {nullptr};
    decltype(&gs1_create_runtime) create_runtime {nullptr};
    decltype(&gs1_destroy_runtime) destroy_runtime {nullptr};
    decltype(&gs1_submit_gameplay_action) submit_gameplay_action {nullptr};
    decltype(&gs1_submit_site_move_direction) submit_site_move_direction {nullptr};
    decltype(&gs1_submit_site_action_request) submit_site_action_request {nullptr};
    decltype(&gs1_submit_site_action_cancel) submit_site_action_cancel {nullptr};
    decltype(&gs1_submit_site_context_request) submit_site_context_request {nullptr};
    decltype(&gs1_submit_site_inventory_slot_tap) submit_site_inventory_slot_tap {nullptr};
    decltype(&gs1_submit_site_scene_ready) submit_site_scene_ready {nullptr};
    decltype(&gs1_run_phase1) run_phase1 {nullptr};
    decltype(&gs1_run_phase2) run_phase2 {nullptr};
    decltype(&gs1_get_game_state_view) get_game_state_view {nullptr};
    decltype(&gs1_query_site_tile_view) query_site_tile_view {nullptr};
    decltype(&gs1_get_runtime_profiling_snapshot) get_runtime_profiling_snapshot {nullptr};
    decltype(&gs1_reset_runtime_profiling) reset_runtime_profiling {nullptr};
    decltype(&gs1_set_runtime_profile_system_enabled) set_runtime_profile_system_enabled {nullptr};
};

class RuntimeDllLoader final
{
public:
    RuntimeDllLoader() noexcept = default;
    RuntimeDllLoader(const RuntimeDllLoader&) = delete;
    RuntimeDllLoader& operator=(const RuntimeDllLoader&) = delete;
    ~RuntimeDllLoader() noexcept;

    [[nodiscard]] bool load(const wchar_t* dll_path);
    void unload() noexcept;

    [[nodiscard]] const RuntimeApi& api() const noexcept { return api_; }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

private:
    template <typename FunctionPointer>
    [[nodiscard]] bool load_symbol(FunctionPointer& out_function, const char* symbol_name);

private:
    void* module_ {nullptr};
    RuntimeApi api_ {};
    std::string last_error_ {};
};


}  // namespace shared_framework::host
