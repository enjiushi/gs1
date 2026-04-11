#pragma once

#include "gs1/game_api.h"

#include <string>

struct Gs1RuntimeApi final
{
    decltype(&gs1_get_api_version) get_api_version {nullptr};
    decltype(&gs1_get_build_string) get_build_string {nullptr};
    decltype(&gs1_create_runtime) create_runtime {nullptr};
    decltype(&gs1_destroy_runtime) destroy_runtime {nullptr};
    decltype(&gs1_submit_host_events) submit_host_events {nullptr};
    decltype(&gs1_submit_feedback_events) submit_feedback_events {nullptr};
    decltype(&gs1_run_phase1) run_phase1 {nullptr};
    decltype(&gs1_run_phase2) run_phase2 {nullptr};
    decltype(&gs1_pop_engine_command) pop_engine_command {nullptr};
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

    [[nodiscard]] const Gs1RuntimeApi& api() const noexcept { return api_; }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

private:
    template <typename FunctionPointer>
    [[nodiscard]] bool load_symbol(FunctionPointer& out_function, const char* symbol_name);

private:
    void* module_ {nullptr};
    Gs1RuntimeApi api_ {};
    std::string last_error_ {};
};
