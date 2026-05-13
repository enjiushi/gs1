#include "host/runtime_session.h"

#include <cassert>

Gs1RuntimeSession::~Gs1RuntimeSession() noexcept
{
    stop();
}

bool Gs1RuntimeSession::start(
    const std::filesystem::path& gameplay_dll_path,
    const std::filesystem::path& project_config_root,
    const Gs1AdapterConfigBlob* adapter_config,
    const double fixed_step_seconds)
{
    stop();

    if (!loader_.load(gameplay_dll_path.c_str()))
    {
        last_error_ = loader_.last_error();
        return false;
    }

    const Gs1RuntimeApi& api = loader_.api();
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

void Gs1RuntimeSession::stop() noexcept
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

bool Gs1RuntimeSession::run_phase1(double delta_seconds, Gs1Phase1Result& out_phase1)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const Gs1RuntimeApi& api = loader_.api();
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

bool Gs1RuntimeSession::run_phase2(Gs1Phase2Result& out_phase2)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const Gs1RuntimeApi& api = loader_.api();
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

bool Gs1RuntimeSession::update(double delta_seconds, Gs1Phase1Result& out_phase1, Gs1Phase2Result& out_phase2)
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

bool Gs1RuntimeSession::submit_host_messages(const Gs1HostMessage* messages, std::uint32_t message_count)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    if (message_count > 0U && messages == nullptr)
    {
        last_error_ = "Host message buffer is null.";
        return false;
    }

    const Gs1RuntimeApi& api = loader_.api();
    if (api.submit_host_messages == nullptr)
    {
        last_error_ = "Gameplay DLL is missing host message entry points.";
        return false;
    }

    const Gs1Status status = api.submit_host_messages(runtime_, messages, message_count);
    if (status != GS1_STATUS_OK)
    {
        last_error_ = "gs1_submit_host_messages failed with status " + std::to_string(static_cast<unsigned>(status));
        return false;
    }

    last_error_.clear();
    return true;
}

bool Gs1RuntimeSession::submit_host_events(const Gs1HostEvent* messages, std::uint32_t message_count)
{
    return submit_host_messages(messages, message_count);
}

bool Gs1RuntimeSession::pop_runtime_message(Gs1RuntimeMessage& out_message)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const Gs1RuntimeApi& api = loader_.api();
    if (api.pop_runtime_message == nullptr)
    {
        last_error_ = "Gameplay DLL is missing runtime message entry points.";
        return false;
    }

    out_message = {};
    const Gs1Status status = api.pop_runtime_message(runtime_, &out_message);
    if (status == GS1_STATUS_OK)
    {
        last_error_.clear();
        return true;
    }
    if (status == GS1_STATUS_BUFFER_EMPTY)
    {
        last_error_.clear();
        return false;
    }

    last_error_ = "gs1_pop_runtime_message failed with status " + std::to_string(static_cast<unsigned>(status));
    return false;
}

bool Gs1RuntimeSession::pop_engine_message(Gs1EngineMessage& out_message)
{
    return pop_runtime_message(out_message);
}

bool Gs1RuntimeSession::get_game_state_view(Gs1GameStateView& out_view)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const Gs1RuntimeApi& api = loader_.api();
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

bool Gs1RuntimeSession::query_site_tile_view(std::uint32_t tile_index, Gs1SiteTileView& out_tile)
{
    if (runtime_ == nullptr)
    {
        last_error_ = "Runtime session is not running.";
        return false;
    }

    const Gs1RuntimeApi& api = loader_.api();
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

