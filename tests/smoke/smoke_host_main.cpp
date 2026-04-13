#include "smoke_engine_host.h"
#include "runtime_dll_loader.h"
#include "smoke_log.h"
#include "smoke_script_runner.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    const std::filesystem::path repo_root = std::filesystem::current_path();
    const std::filesystem::path dll_path = argc > 1
        ? std::filesystem::path(argv[1])
        : (repo_root / "build" / "Debug" / "gs1_game.dll");
    const std::string script_path = argc > 2
        ? argv[2]
        : "tests/smoke/scripts/main_menu_to_site_active.smoke";

    const auto log_path = repo_root / "out" / "logs" / "smoke_host_latest.log";
    const bool logging_ready = smoke_log::initialize_file_sink(log_path);
    if (!logging_ready)
    {
        smoke_log::errorf("Failed to open log file: %s\n", log_path.string().c_str());
    }
    else
    {
        smoke_log::infof("Writing smoke host log to %s\n", log_path.string().c_str());
    }

    RuntimeDllLoader loader {};
    if (!loader.load(dll_path.c_str()))
    {
        smoke_log::errorf("%s\n", loader.last_error().c_str());
        smoke_log::shutdown_file_sink();
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

    SmokeEngineHost smoke_host {api, runtime};
    SmokeScriptRunner runner {};
    const bool loaded = runner.load_file(script_path);

    constexpr double k_frame_delta_seconds = 1.0 / 60.0;
    constexpr std::uint32_t k_max_smoke_frames = 600U;

    bool succeeded = loaded;
    std::uint32_t executed_frames = 0U;

    while (succeeded && (runner.has_next_directive() || smoke_host.has_inflight_script_directive()))
    {
        if (executed_frames >= k_max_smoke_frames)
        {
            smoke_log::errorf("Smoke test timed out after %u frames.\n", k_max_smoke_frames);
            succeeded = false;
            break;
        }

        if (!smoke_host.has_inflight_script_directive())
        {
            const auto next_directive = runner.pop_next_directive();
            if (next_directive.has_value())
            {
                const bool accepted = smoke_host.set_inflight_script_directive(std::move(next_directive.value()));
                assert(accepted);
            }
        }

        smoke_host.update(k_frame_delta_seconds);
        executed_frames += 1U;

        if (smoke_host.script_failed())
        {
            succeeded = false;
            break;
        }
    }

    api.destroy_runtime(runtime);
    smoke_log::shutdown_file_sink();
    return succeeded ? 0 : 1;
}
