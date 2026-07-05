#pragma once

#include "gs1/game_api.h"
#include "shared_framework/runtime/api/runtime_api.h"

#include <string>

namespace gs1::host {
using RuntimeHostHandle = void;

struct RuntimeBootstrapApi final
{
    std::uint32_t (*get_api_version)() {nullptr};
    const char* (*get_build_string)() {nullptr};
    GsStatus (*create_runtime)(const GsRuntimeCreateDesc* create_desc, RuntimeHostHandle** out_runtime) {nullptr};
    void (*destroy_runtime)(RuntimeHostHandle* runtime) {nullptr};
    GsStatus (*run_phase1)(
        RuntimeHostHandle* runtime,
        const GsPhase1Request* request,
        GsPhase1Result* out_result) {nullptr};
    GsStatus (*run_phase2)(
        RuntimeHostHandle* runtime,
        const GsPhase2Request* request,
        GsPhase2Result* out_result) {nullptr};
};

class RuntimeBootstrapDllLoader final
{
public:
    RuntimeBootstrapDllLoader() noexcept = default;
    RuntimeBootstrapDllLoader(const RuntimeBootstrapDllLoader&) = delete;
    RuntimeBootstrapDllLoader& operator=(const RuntimeBootstrapDllLoader&) = delete;
    ~RuntimeBootstrapDllLoader() noexcept;

    [[nodiscard]] bool load(const wchar_t* dll_path);
    void unload() noexcept;

    [[nodiscard]] bool load_required_symbol(void** out_symbol, const char* symbol_name);
    [[nodiscard]] const RuntimeBootstrapApi& api() const noexcept { return api_; }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

private:
    template <typename FunctionPointer>
    [[nodiscard]] bool load_symbol(FunctionPointer& out_function, const char* symbol_name);

    void* module_ {nullptr};
    RuntimeBootstrapApi api_ {};
    std::string last_error_ {};
};

struct RuntimeApi final
{
    RuntimeBootstrapApi bootstrap {};
    Gs1Status (*submit_gameplay_action)(RuntimeHostHandle* runtime, const Gs1GameplayAction* action) {nullptr};
    Gs1Status (*submit_site_move_direction)(
        RuntimeHostHandle* runtime,
        const Gs1SiteMoveDirectionCommand* command) {nullptr};
    Gs1Status (*submit_site_action_request)(
        RuntimeHostHandle* runtime,
        const Gs1SiteActionRequestCommand* command) {nullptr};
    Gs1Status (*submit_site_action_cancel)(
        RuntimeHostHandle* runtime,
        const Gs1SiteActionCancelCommand* command) {nullptr};
    Gs1Status (*submit_site_context_request)(
        RuntimeHostHandle* runtime,
        const Gs1SiteContextRequestCommand* command) {nullptr};
    Gs1Status (*submit_site_inventory_slot_tap)(
        RuntimeHostHandle* runtime,
        const Gs1SiteInventorySlotTapCommand* command) {nullptr};
    Gs1Status (*submit_site_scene_ready)(RuntimeHostHandle* runtime) {nullptr};
    Gs1Status (*get_game_state_view)(RuntimeHostHandle* runtime, Gs1GameStateView* out_view) {nullptr};
    Gs1Status (*query_site_tile_view)(
        RuntimeHostHandle* runtime,
        std::uint32_t tile_index,
        Gs1SiteTileView* out_tile) {nullptr};
    Gs1Status (*get_runtime_profiling_snapshot)(
        RuntimeHostHandle* runtime,
        Gs1RuntimeProfilingSnapshot* out_snapshot) {nullptr};
    Gs1Status (*reset_runtime_profiling)(RuntimeHostHandle* runtime) {nullptr};
    Gs1Status (*set_runtime_profile_system_enabled)(
        RuntimeHostHandle* runtime,
        Gs1RuntimeProfileSystemId system_id,
        std::uint8_t enabled) {nullptr};
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
    [[nodiscard]] const std::string& last_error() const noexcept { return bootstrap_loader_.last_error(); }

private:
    template <typename FunctionPointer>
    [[nodiscard]] bool load_symbol(FunctionPointer& out_function, const char* symbol_name);

    RuntimeBootstrapDllLoader bootstrap_loader_ {};
    RuntimeApi api_ {};
};


}  // namespace gs1::host
