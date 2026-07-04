#include "shared_framework/host/runtime_session.h"

#include <cassert>

namespace shared_framework::host {

RuntimeSession::~RuntimeSession() noexcept
{
    stop();
}

bool RuntimeSession::start(
    const std::filesystem::path& gameplay_dll_path,
    const std::filesystem::path& project_config_root,
    const AdapterConfigBlob* adapter_config,
    const double fixed_step_seconds)
{
    stop();

    if (!loader_.load(gameplay_dll_path.c_str()))
    {
        last_error_ = loader_.last_error();
        return false;
    }

    const RuntimeApi& api = loader_.api();
    if (api.get_api_version == nullptr || api.create_runtime == nullptr || api.destroy_runtime == nullptr)
    {
        last_error_ = "Gameplay DLL is missing required runtime entry points.";
        loader_.unload();
        return false;
    }

    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = api.get_api_version();
    create_desc.fixed_step_seconds = fixed_step_seconds;
    project_config_root_utf8_ = project_config_root.string();
    create_desc.project_config_root_utf8 = project_config_root_utf8_.c_str();
    adapter_config_json_utf8_ = adapter_config != nullptr ? adapter_config->json_utf8 : std::string {};
    create_desc.adapter_config_json_utf8 = adapter_config_json_utf8_.empty()
        ? nullptr
        : adapter_config_json_utf8_.c_str();

    Gs1RuntimeHandle* runtime = nullptr;
    const Gs1Status status = api.create_runtime(&create_desc, &runtime);
    if (status != GS1_STATUS_OK || runtime == nullptr)
    {
        last_error_ = "gs1_create_runtime failed with status " + std::to_string(static_cast<unsigned>(status));
        loader_.unload();
        return false;
    }

    runtime_ = runtime;
    gameplay_dll_path_ = gameplay_dll_path;
    last_error_.clear();
    return true;
}

void RuntimeSession::stop() noexcept
{
    if (runtime_ != nullptr)
    {
        assert(loader_.api().destroy_runtime != nullptr);
        loader_.api().destroy_runtime(runtime_);
    }

    runtime_ = nullptr;
    gameplay_dll_path_.clear();
    project_config_root_utf8_.clear();
    adapter_config_json_utf8_.clear();
    loader_.unload();
}

bool RuntimeSession::run_phase1(double delta_seconds, Gs1Phase1Result& out_phase1)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const RuntimeApi& api = loader_.api();
    if (api.run_phase1 == nullptr)
    {
        last_error_ = "Gameplay DLL is missing phase 1 entry points.";
        return false;
    }

    Gs1Phase1Request phase1_request {};
    phase1_request.struct_size = sizeof(Gs1Phase1Request);
    phase1_request.real_delta_seconds = delta_seconds;

    out_phase1 = {};
    Gs1Status status = api.run_phase1(runtime_, &phase1_request, &out_phase1);
    if (status != GS1_STATUS_OK)
    {
        last_error_ = "gs1_run_phase1 failed with status " + std::to_string(static_cast<unsigned>(status));
        return false;
    }

    last_error_.clear();
    return true;
}

bool RuntimeSession::run_phase2(Gs1Phase2Result& out_phase2)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const RuntimeApi& api = loader_.api();
    if (api.run_phase2 == nullptr)
    {
        last_error_ = "Gameplay DLL is missing phase 2 entry points.";
        return false;
    }

    Gs1Phase2Request phase2_request {};
    phase2_request.struct_size = sizeof(Gs1Phase2Request);

    out_phase2 = {};
    const Gs1Status status = api.run_phase2(runtime_, &phase2_request, &out_phase2);
    if (status != GS1_STATUS_OK)
    {
        last_error_ = "gs1_run_phase2 failed with status " + std::to_string(static_cast<unsigned>(status));
        return false;
    }

    last_error_.clear();
    return true;
}

bool RuntimeSession::update(double delta_seconds, Gs1Phase1Result& out_phase1, Gs1Phase2Result& out_phase2)
{
    if (!run_phase1(delta_seconds, out_phase1))
    {
        return false;
    }

    if (!run_phase2(out_phase2))
    {
        return false;
    }

    last_error_.clear();
    return true;
}

bool RuntimeSession::submit_gameplay_action(const Gs1GameplayAction& action)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const RuntimeApi& api = loader_.api();
    if (api.submit_gameplay_action == nullptr)
    {
        last_error_ = "Gameplay DLL is missing gameplay action entry points.";
        return false;
    }

    const Gs1Status status = api.submit_gameplay_action(runtime_, &action);
    if (status != GS1_STATUS_OK)
    {
        last_error_ = "gs1_submit_gameplay_action failed with status " + std::to_string(static_cast<unsigned>(status));
        return false;
    }

    last_error_.clear();
    return true;
}

bool RuntimeSession::submit_site_move_direction(const Gs1SiteMoveDirectionCommand& command)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const RuntimeApi& api = loader_.api();
    if (api.submit_site_move_direction == nullptr)
    {
        last_error_ = "Gameplay DLL is missing site move direction entry points.";
        return false;
    }

    const Gs1Status status = api.submit_site_move_direction(runtime_, &command);
    if (status != GS1_STATUS_OK)
    {
        last_error_ = "gs1_submit_site_move_direction failed with status " + std::to_string(static_cast<unsigned>(status));
        return false;
    }

    last_error_.clear();
    return true;
}

bool RuntimeSession::submit_site_action_request(const Gs1SiteActionRequestCommand& command)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const RuntimeApi& api = loader_.api();
    if (api.submit_site_action_request == nullptr)
    {
        last_error_ = "Gameplay DLL is missing site action request entry points.";
        return false;
    }

    const Gs1Status status = api.submit_site_action_request(runtime_, &command);
    if (status != GS1_STATUS_OK)
    {
        last_error_ = "gs1_submit_site_action_request failed with status " + std::to_string(static_cast<unsigned>(status));
        return false;
    }

    last_error_.clear();
    return true;
}

bool RuntimeSession::submit_site_action_cancel(const Gs1SiteActionCancelCommand& command)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const RuntimeApi& api = loader_.api();
    if (api.submit_site_action_cancel == nullptr)
    {
        last_error_ = "Gameplay DLL is missing site action cancel entry points.";
        return false;
    }

    const Gs1Status status = api.submit_site_action_cancel(runtime_, &command);
    if (status != GS1_STATUS_OK)
    {
        last_error_ = "gs1_submit_site_action_cancel failed with status " + std::to_string(static_cast<unsigned>(status));
        return false;
    }

    last_error_.clear();
    return true;
}

bool RuntimeSession::submit_site_context_request(const Gs1SiteContextRequestCommand& command)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const RuntimeApi& api = loader_.api();
    if (api.submit_site_context_request == nullptr)
    {
        last_error_ = "Gameplay DLL is missing site context request entry points.";
        return false;
    }

    const Gs1Status status = api.submit_site_context_request(runtime_, &command);
    if (status != GS1_STATUS_OK)
    {
        last_error_ = "gs1_submit_site_context_request failed with status " + std::to_string(static_cast<unsigned>(status));
        return false;
    }

    last_error_.clear();
    return true;
}

bool RuntimeSession::submit_site_inventory_slot_tap(const Gs1SiteInventorySlotTapCommand& command)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const RuntimeApi& api = loader_.api();
    if (api.submit_site_inventory_slot_tap == nullptr)
    {
        last_error_ = "Gameplay DLL is missing site inventory slot tap entry points.";
        return false;
    }

    const Gs1Status status = api.submit_site_inventory_slot_tap(runtime_, &command);
    if (status != GS1_STATUS_OK)
    {
        last_error_ = "gs1_submit_site_inventory_slot_tap failed with status " + std::to_string(static_cast<unsigned>(status));
        return false;
    }

    last_error_.clear();
    return true;
}

bool RuntimeSession::submit_site_scene_ready()
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const RuntimeApi& api = loader_.api();
    if (api.submit_site_scene_ready == nullptr)
    {
        last_error_ = "Gameplay DLL is missing site scene ready entry points.";
        return false;
    }

    const Gs1Status status = api.submit_site_scene_ready(runtime_);
    if (status != GS1_STATUS_OK)
    {
        last_error_ = "gs1_submit_site_scene_ready failed with status " + std::to_string(static_cast<unsigned>(status));
        return false;
    }

    last_error_.clear();
    return true;
}

bool RuntimeSession::get_game_state_view(Gs1GameStateView& out_view)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const RuntimeApi& api = loader_.api();
    if (api.get_game_state_view == nullptr)
    {
        last_error_ = "Gameplay DLL is missing game state view entry points.";
        return false;
    }

    out_view = {};
    out_view.struct_size = sizeof(Gs1GameStateView);
    const Gs1Status status = api.get_game_state_view(runtime_, &out_view);
    if (status != GS1_STATUS_OK)
    {
        last_error_ = "gs1_get_game_state_view failed with status " + std::to_string(static_cast<unsigned>(status));
        return false;
    }

    last_error_.clear();
    return true;
}

bool RuntimeSession::query_site_tile_view(std::uint32_t tile_index, Gs1SiteTileView& out_tile)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const RuntimeApi& api = loader_.api();
    if (api.query_site_tile_view == nullptr)
    {
        last_error_ = "Gameplay DLL is missing site tile query entry points.";
        return false;
    }

    out_tile = {};
    const Gs1Status status = api.query_site_tile_view(runtime_, tile_index, &out_tile);
    if (status != GS1_STATUS_OK)
    {
        last_error_ = "gs1_query_site_tile_view failed with status " + std::to_string(static_cast<unsigned>(status));
        return false;
    }

    last_error_.clear();
    return true;
}

}  // namespace shared_framework::host
