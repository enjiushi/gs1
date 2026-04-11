#include "runtime_dll_loader.h"
#include "smoke_engine_host.h"
#include "smoke_live_http_server.h"
#include "smoke_log.h"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>

namespace
{
struct LiveSession final
{
    explicit LiveSession(const Gs1RuntimeApi& api, Gs1RuntimeHandle* runtime) noexcept
        : host(api, runtime, SmokeEngineHost::LogMode::ActivityOnly)
    {
        input_snapshot.struct_size = sizeof(Gs1InputSnapshot);
    }

    SmokeEngineHost host;
    Gs1InputSnapshot input_snapshot {};
    std::mutex mutex {};
};

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
    if (value == "START_SITE_ATTEMPT")
    {
        return GS1_UI_ACTION_START_SITE_ATTEMPT;
    }
    if (value == "RETURN_TO_REGIONAL_MAP")
    {
        return GS1_UI_ACTION_RETURN_TO_REGIONAL_MAP;
    }

    return std::nullopt;
}

void run_live_mode(
    const Gs1RuntimeApi& api,
    Gs1RuntimeHandle* runtime,
    const std::filesystem::path& repo_root,
    std::uint16_t preferred_port)
{
    LiveSession session {api, runtime};

    SmokeLiveHttpServer server {
        repo_root,
        [&session]() {
            std::scoped_lock lock {session.mutex};
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

            std::scoped_lock lock {session.mutex};
            session.host.queue_ui_action(action);
        },
        [&session](const std::string& body) {
            std::scoped_lock lock {session.mutex};
            session.input_snapshot.move_x = extract_number_field<float>(body, "moveX").value_or(0.0f);
            session.input_snapshot.move_y = extract_number_field<float>(body, "moveY").value_or(0.0f);
            session.input_snapshot.cursor_world_x = extract_number_field<float>(body, "cursorWorldX").value_or(0.0f);
            session.input_snapshot.cursor_world_y = extract_number_field<float>(body, "cursorWorldY").value_or(0.0f);
            session.input_snapshot.buttons_down_mask =
                extract_number_field<std::uint32_t>(body, "buttonsDownMask").value_or(0U);
            session.input_snapshot.buttons_pressed_mask =
                extract_number_field<std::uint32_t>(body, "buttonsPressedMask").value_or(0U);
            session.input_snapshot.buttons_released_mask =
                extract_number_field<std::uint32_t>(body, "buttonsReleasedMask").value_or(0U);
        }};

    if (!server.start(preferred_port))
    {
        throw std::runtime_error("Failed to start local HTTP server for visual smoke host.");
    }

    smoke_log::infof("Visual smoke host running at http://127.0.0.1:%u/\n", static_cast<unsigned>(server.port()));
    smoke_log::infof("Press Ctrl+C to stop the host.\n");

    constexpr double k_frame_delta_seconds = 1.0 / 60.0;
    constexpr auto k_frame_time = std::chrono::milliseconds(16);

    auto next_tick = std::chrono::steady_clock::now();

    while (true)
    {
        next_tick += k_frame_time;

        Gs1InputSnapshot input_snapshot {};
        {
            std::scoped_lock lock {session.mutex};
            input_snapshot = session.input_snapshot;
            input_snapshot.struct_size = sizeof(Gs1InputSnapshot);
            session.host.update(k_frame_delta_seconds, &input_snapshot);
            session.input_snapshot.buttons_pressed_mask = 0U;
            session.input_snapshot.buttons_released_mask = 0U;
        }

        std::this_thread::sleep_until(next_tick);
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

    for (int index = 2; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--port" && (index + 1) < argc)
        {
            preferred_port = static_cast<std::uint16_t>(std::strtoul(argv[++index], nullptr, 10));
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
    create_desc.fixed_step_seconds = 0.25;

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
        run_live_mode(api, runtime, repo_root, preferred_port);
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
