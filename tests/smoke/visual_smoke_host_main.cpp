#include "runtime_dll_loader.h"
#include "smoke_engine_host.h"
#include "smoke_live_http_server.h"
#include "smoke_log.h"
#include "visual_smoke_silent_onboarding.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <Windows.h>
#include <mmsystem.h>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>

namespace
{
struct SiteControlState final
{
    bool has_move_input {false};
    float world_move_x {0.0f};
    float world_move_y {0.0f};
    float world_move_z {0.0f};
};

struct LiveSession final
{
    explicit LiveSession(
        const Gs1RuntimeApi& api,
        Gs1RuntimeHandle* runtime,
        SmokeEngineHost::LogMode log_mode) noexcept
        : host(api, runtime, log_mode)
    {
    }

    SmokeEngineHost host;
    SiteControlState site_control {};
    std::mutex site_control_mutex {};
};

class WindowsTimerResolution final
{
public:
    explicit WindowsTimerResolution(UINT period_milliseconds) noexcept
        : period_milliseconds_(period_milliseconds)
        , active_(timeBeginPeriod(period_milliseconds_) == TIMERR_NOERROR)
    {
    }

    ~WindowsTimerResolution()
    {
        if (active_)
        {
            timeEndPeriod(period_milliseconds_);
        }
    }

private:
    UINT period_milliseconds_ {0U};
    bool active_ {false};
};

double duration_milliseconds(std::chrono::steady_clock::duration duration) noexcept
{
    return std::chrono::duration<double, std::milli>(duration).count();
}

std::string build_frame_perf_json(
    std::uint64_t frame_number,
    double visual_host_ms,
    double gameplay_dll_ms)
{
    std::ostringstream stream {};
    stream << std::fixed << std::setprecision(3);
    stream << "{\"frameNumber\":" << frame_number
           << ",\"visualHostMs\":" << visual_host_ms
           << ",\"gameplayDllMs\":" << gameplay_dll_ms
           << "}";
    return stream.str();
}

std::optional<std::string> extract_string_field(const std::string& body, const char* key)
{
    const std::string pattern = std::string("\"") + key + "\"";
    const auto key_pos = body.find(pattern);
    if (key_pos == std::string::npos)
    {
        return std::nullopt;
    }

    const auto colon_pos = body.find(':', key_pos + pattern.size());
    if (colon_pos == std::string::npos)
    {
        return std::nullopt;
    }

    const auto first_quote = body.find('"', colon_pos + 1U);
    if (first_quote == std::string::npos)
    {
        return std::nullopt;
    }

    const auto second_quote = body.find('"', first_quote + 1U);
    if (second_quote == std::string::npos)
    {
        return std::nullopt;
    }

    return body.substr(first_quote + 1U, second_quote - first_quote - 1U);
}

template <typename NumberType>
std::optional<NumberType> extract_number_field(const std::string& body, const char* key)
{
    const std::string pattern = std::string("\"") + key + "\"";
    const auto key_pos = body.find(pattern);
    if (key_pos == std::string::npos)
    {
        return std::nullopt;
    }

    const auto colon_pos = body.find(':', key_pos + pattern.size());
    if (colon_pos == std::string::npos)
    {
        return std::nullopt;
    }

    std::size_t value_start = colon_pos + 1U;
    while (value_start < body.size() &&
        (body[value_start] == ' ' || body[value_start] == '\t' || body[value_start] == '\r' || body[value_start] == '\n'))
    {
        value_start += 1U;
    }

    std::size_t value_end = value_start;
    while (value_end < body.size() &&
        body[value_end] != ',' &&
        body[value_end] != '}' &&
        body[value_end] != '\r' &&
        body[value_end] != '\n')
    {
        value_end += 1U;
    }

    const auto raw_value = body.substr(value_start, value_end - value_start);
    if constexpr (std::is_floating_point_v<NumberType>)
    {
        return static_cast<NumberType>(std::strtod(raw_value.c_str(), nullptr));
    }
    else
    {
        return static_cast<NumberType>(std::strtoull(raw_value.c_str(), nullptr, 10));
    }
}

std::optional<Gs1UiActionType> parse_ui_action_type(const std::string& value)
{
    if (value == "START_NEW_CAMPAIGN")
    {
        return GS1_UI_ACTION_START_NEW_CAMPAIGN;
    }
    if (value == "SELECT_DEPLOYMENT_SITE")
    {
        return GS1_UI_ACTION_SELECT_DEPLOYMENT_SITE;
    }
    if (value == "CLEAR_DEPLOYMENT_SITE_SELECTION")
    {
        return GS1_UI_ACTION_CLEAR_DEPLOYMENT_SITE_SELECTION;
    }
    if (value == "START_SITE_ATTEMPT")
    {
        return GS1_UI_ACTION_START_SITE_ATTEMPT;
    }
    if (value == "RETURN_TO_REGIONAL_MAP")
    {
        return GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP;
    }
    if (value == "ACCEPT_TASK")
    {
        return GS1_UI_ACTION_ACCEPT_TASK;
    }
    if (value == "CLAIM_TASK_REWARD")
    {
        return GS1_UI_ACTION_CLAIM_TASK_REWARD;
    }
    if (value == "BUY_PHONE_LISTING")
    {
        return GS1_UI_ACTION_BUY_PHONE_LISTING;
    }
    if (value == "SELL_PHONE_LISTING")
    {
        return GS1_UI_ACTION_SELL_PHONE_LISTING;
    }
    if (value == "ADD_PHONE_LISTING_TO_CART")
    {
        return GS1_UI_ACTION_ADD_PHONE_LISTING_TO_CART;
    }
    if (value == "REMOVE_PHONE_LISTING_FROM_CART")
    {
        return GS1_UI_ACTION_REMOVE_PHONE_LISTING_FROM_CART;
    }
    if (value == "CHECKOUT_PHONE_CART")
    {
        return GS1_UI_ACTION_CHECKOUT_PHONE_CART;
    }
    if (value == "SET_PHONE_PANEL_SECTION")
    {
        return GS1_UI_ACTION_SET_PHONE_PANEL_SECTION;
    }
    if (value == "CLOSE_PHONE_PANEL")
    {
        return GS1_UI_ACTION_CLOSE_PHONE_PANEL;
    }
    if (value == "OPEN_SITE_PROTECTION_SELECTOR")
    {
        return GS1_UI_ACTION_OPEN_SITE_PROTECTION_SELECTOR;
    }
    if (value == "CLOSE_SITE_PROTECTION_UI")
    {
        return GS1_UI_ACTION_CLOSE_SITE_PROTECTION_UI;
    }
    if (value == "SET_SITE_PROTECTION_OVERLAY_MODE")
    {
        return GS1_UI_ACTION_SET_SITE_PROTECTION_OVERLAY_MODE;
    }
    if (value == "OPEN_REGIONAL_MAP_TECH_TREE")
    {
        return GS1_UI_ACTION_OPEN_REGIONAL_MAP_TECH_TREE;
    }
    if (value == "CLOSE_REGIONAL_MAP_TECH_TREE")
    {
        return GS1_UI_ACTION_CLOSE_REGIONAL_MAP_TECH_TREE;
    }
    if (value == "CLAIM_TECHNOLOGY_NODE")
    {
        return GS1_UI_ACTION_CLAIM_TECHNOLOGY_NODE;
    }
    if (value == "SELECT_TECH_TREE_FACTION_TAB")
    {
        return GS1_UI_ACTION_SELECT_TECH_TREE_FACTION_TAB;
    }
    if (value == "USE_INVENTORY_ITEM")
    {
        return GS1_UI_ACTION_USE_INVENTORY_ITEM;
    }
    if (value == "TRANSFER_INVENTORY_ITEM")
    {
        return GS1_UI_ACTION_TRANSFER_INVENTORY_ITEM;
    }
    if (value == "HIRE_CONTRACTOR")
    {
        return GS1_UI_ACTION_HIRE_CONTRACTOR;
    }
    if (value == "PURCHASE_SITE_UNLOCKABLE")
    {
        return GS1_UI_ACTION_PURCHASE_SITE_UNLOCKABLE;
    }
    if (value == "END_SITE_MODIFIER")
    {
        return GS1_UI_ACTION_END_SITE_MODIFIER;
    }

    return std::nullopt;
}

std::optional<Gs1SiteActionKind> parse_site_action_kind(const std::string& value)
{
    if (value == "PLANT")
    {
        return GS1_SITE_ACTION_PLANT;
    }
    if (value == "BUILD")
    {
        return GS1_SITE_ACTION_BUILD;
    }
    if (value == "REPAIR")
    {
        return GS1_SITE_ACTION_REPAIR;
    }
    if (value == "WATER")
    {
        return GS1_SITE_ACTION_WATER;
    }
    if (value == "CLEAR_BURIAL")
    {
        return GS1_SITE_ACTION_CLEAR_BURIAL;
    }
    if (value == "CRAFT")
    {
        return GS1_SITE_ACTION_CRAFT;
    }
    if (value == "DRINK")
    {
        return GS1_SITE_ACTION_DRINK;
    }
    if (value == "EAT")
    {
        return GS1_SITE_ACTION_EAT;
    }
    if (value == "HARVEST")
    {
        return GS1_SITE_ACTION_HARVEST;
    }
    if (value == "EXCAVATE")
    {
        return GS1_SITE_ACTION_EXCAVATE;
    }

    return std::nullopt;
}

std::optional<Gs1InventoryViewEventKind> parse_inventory_view_event_kind(const std::string& value)
{
    if (value == "OPEN_SNAPSHOT")
    {
        return GS1_INVENTORY_VIEW_EVENT_OPEN_SNAPSHOT;
    }
    if (value == "CLOSE")
    {
        return GS1_INVENTORY_VIEW_EVENT_CLOSE;
    }

    return std::nullopt;
}

void run_live_mode(
    const Gs1RuntimeApi& api,
    Gs1RuntimeHandle* runtime,
    const std::filesystem::path& repo_root,
    std::uint16_t preferred_port,
    SmokeEngineHost::LogMode log_mode)
{
    LiveSession session {api, runtime, log_mode};
    WindowsTimerResolution timer_resolution {1U};

    SmokeLiveHttpServer server {
        repo_root,
        [&session]() {
            return session.host.build_live_state_json();
        },
        [&session](const std::string& body) {
            const auto action_type_name = extract_string_field(body, "type");
            if (!action_type_name.has_value())
            {
                return;
            }

            const auto action_type = parse_ui_action_type(action_type_name.value());
            if (!action_type.has_value())
            {
                return;
            }

            Gs1UiAction action {};
            action.type = action_type.value();
            action.target_id = extract_number_field<std::uint32_t>(body, "targetId").value_or(0U);
            action.arg0 = extract_number_field<std::uint64_t>(body, "arg0").value_or(0ULL);
            action.arg1 = extract_number_field<std::uint64_t>(body, "arg1").value_or(0ULL);

            session.host.queue_ui_action(action);
        },
        [&session](const std::string& body) {
            const auto action_kind_name = extract_string_field(body, "actionKind");
            if (!action_kind_name.has_value())
            {
                return;
            }

            const auto action_kind = parse_site_action_kind(action_kind_name.value());
            if (!action_kind.has_value())
            {
                return;
            }

            Gs1HostEventSiteActionRequestData action {};
            action.action_kind = action_kind.value();
            action.flags = static_cast<std::uint8_t>(
                extract_number_field<std::uint32_t>(body, "flags").value_or(0U));
            action.quantity = static_cast<std::uint16_t>(
                extract_number_field<std::uint32_t>(body, "quantity").value_or(1U));
            action.target_tile_x = extract_number_field<std::int32_t>(body, "targetTileX").value_or(0);
            action.target_tile_y = extract_number_field<std::int32_t>(body, "targetTileY").value_or(0);
            action.primary_subject_id =
                extract_number_field<std::uint32_t>(body, "primarySubjectId").value_or(0U);
            action.secondary_subject_id =
                extract_number_field<std::uint32_t>(body, "secondarySubjectId").value_or(0U);
            action.item_id = extract_number_field<std::uint32_t>(body, "itemId").value_or(0U);

            session.host.queue_site_action_request(action);
        },
        [&session](const std::string& body) {
            Gs1HostEventSiteActionCancelData action {};
            action.action_id = extract_number_field<std::uint32_t>(body, "actionId").value_or(0U);
            action.flags = extract_number_field<std::uint32_t>(body, "flags").value_or(0U);

            session.host.queue_site_action_cancel(action);
        },
        [&session](const std::string& body) {
            const auto event_kind_name = extract_string_field(body, "eventKind");
            if (!event_kind_name.has_value())
            {
                return;
            }

            const auto event_kind = parse_inventory_view_event_kind(event_kind_name.value());
            if (!event_kind.has_value())
            {
                return;
            }

            Gs1HostEventSiteStorageViewData request {};
            request.storage_id = extract_number_field<std::uint32_t>(body, "storageId").value_or(0U);
            request.event_kind = event_kind.value();

            session.host.queue_site_storage_view(request);
        },
        [&session](const std::string& body) {
            Gs1HostEventSiteContextRequestData request {};
            request.tile_x = extract_number_field<std::int32_t>(body, "tileX").value_or(0);
            request.tile_y = extract_number_field<std::int32_t>(body, "tileY").value_or(0);
            request.flags = extract_number_field<std::uint32_t>(body, "flags").value_or(0U);

            session.host.queue_site_context_request(request);
        },
        [&session](const std::string& body) {
            std::scoped_lock lock {session.site_control_mutex};
            session.site_control.has_move_input =
                extract_number_field<std::uint32_t>(body, "hasMoveInput").value_or(0U) != 0U;
            session.site_control.world_move_x = extract_number_field<float>(body, "worldMoveX").value_or(0.0f);
            session.site_control.world_move_y = extract_number_field<float>(body, "worldMoveY").value_or(0.0f);
            session.site_control.world_move_z = extract_number_field<float>(body, "worldMoveZ").value_or(0.0f);

            if (!session.site_control.has_move_input)
            {
                session.site_control.world_move_x = 0.0f;
                session.site_control.world_move_y = 0.0f;
                session.site_control.world_move_z = 0.0f;
            }
        },
        [](const std::string& body) {
            smoke_log::infof("[CLIENT] %s\n", body.c_str());
        }};

    if (!server.start(preferred_port))
    {
        throw std::runtime_error("Failed to start local HTTP server for visual smoke host.");
    }

    smoke_log::infof("Visual smoke host running at http://127.0.0.1:%u/\n", static_cast<unsigned>(server.port()));
    smoke_log::infof("Press Ctrl+C to stop the host.\n");

    constexpr double k_frame_delta_seconds = 1.0 / 60.0;
    constexpr auto k_frame_time =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(
            k_frame_delta_seconds));

    while (true)
    {
        const auto frame_start = std::chrono::steady_clock::now();

        SiteControlState site_control_snapshot {};
        {
            std::scoped_lock lock {session.site_control_mutex};
            site_control_snapshot = session.site_control;
        }

        std::vector<std::string> pending_patches {};
        if (site_control_snapshot.has_move_input)
        {
            session.host.queue_site_move_direction(
                site_control_snapshot.world_move_x,
                site_control_snapshot.world_move_y,
                site_control_snapshot.world_move_z);
        }
        session.host.update(k_frame_delta_seconds);
        const auto frame_active_end = std::chrono::steady_clock::now();
        const auto frame_timing = session.host.last_frame_timing();
        const double gameplay_dll_ms =
            std::chrono::duration<double, std::milli>(std::chrono::duration<double>(
                frame_timing.gameplay_dll_seconds))
                .count();
        const double frame_active_ms = duration_milliseconds(frame_active_end - frame_start);
        const double visual_host_ms =
            frame_active_ms > gameplay_dll_ms ? (frame_active_ms - gameplay_dll_ms) : 0.0;
        const auto perf_event = build_frame_perf_json(
            session.host.frame_number(),
            visual_host_ms,
            gameplay_dll_ms);
        pending_patches = session.host.consume_pending_live_state_patches();

        for (const auto& patch : pending_patches)
        {
            server.publish_event("state-patch", patch);
        }
        server.publish_event("frame-perf", perf_event);

        std::this_thread::sleep_until(frame_start + k_frame_time);
    }
}
}  // namespace

int main(int argc, char** argv)
{
    const std::filesystem::path repo_root = std::filesystem::current_path();
    const std::filesystem::path dll_path = argc > 1
        ? std::filesystem::path(argv[1])
        : (repo_root / "build" / "Debug" / "gs1_game.dll");

    std::uint16_t preferred_port = 0U;
    bool verbose_logging = false;
    bool silent_onboarding = false;
    VisualSmokeSilentOnboardingOptions silent_onboarding_options {};

    for (int index = 2; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--port" && (index + 1) < argc)
        {
            preferred_port = static_cast<std::uint16_t>(std::strtoul(argv[++index], nullptr, 10));
            continue;
        }

        if (argument == "--verbose")
        {
            verbose_logging = true;
            continue;
        }

        if (argument == "--silent-onboarding")
        {
            silent_onboarding = true;
            continue;
        }

        if (argument == "--max-frames" && (index + 1) < argc)
        {
            silent_onboarding_options.max_frames = static_cast<std::uint32_t>(
                std::strtoul(argv[++index], nullptr, 10));
        }
    }

    RuntimeDllLoader loader {};
    if (!loader.load(dll_path.c_str()))
    {
        smoke_log::errorf("%s\n", loader.last_error().c_str());
        return 1;
    }

    const auto& api = loader.api();

    Gs1RuntimeCreateDesc create_desc {};
    create_desc.struct_size = sizeof(Gs1RuntimeCreateDesc);
    create_desc.api_version = api.get_api_version();
    create_desc.fixed_step_seconds = 1.0 / 60.0;

    Gs1RuntimeHandle* runtime = nullptr;
    assert(api.create_runtime(&create_desc, &runtime) == GS1_STATUS_OK);
    assert(runtime != nullptr);

    const auto log_path = repo_root / "out" / "logs" / "visual_smoke_host_latest.log";
    const bool logging_ready = smoke_log::initialize_file_sink(log_path);
    if (!logging_ready)
    {
        smoke_log::errorf("Failed to open log file: %s\n", log_path.string().c_str());
    }
    else
    {
        smoke_log::infof("Writing visual host log to %s\n", log_path.string().c_str());
    }

    int exit_code = 0;
    try
    {
        const auto log_mode = verbose_logging
            ? SmokeEngineHost::LogMode::Verbose
            : SmokeEngineHost::LogMode::ActivityOnly;
        if (silent_onboarding)
        {
            exit_code = run_visual_smoke_silent_onboarding(api, runtime, log_mode, silent_onboarding_options);
        }
        else
        {
            run_live_mode(api, runtime, repo_root, preferred_port, log_mode);
        }
    }
    catch (const std::exception& exception)
    {
        smoke_log::errorf("%s\n", exception.what());
        exit_code = 1;
    }

    api.destroy_runtime(runtime);
    smoke_log::shutdown_file_sink();
    return exit_code;
}
