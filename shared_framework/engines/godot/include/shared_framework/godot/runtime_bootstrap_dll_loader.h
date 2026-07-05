#pragma once

#include "shared_framework/runtime/api/runtime_api.h"

#include <string>

namespace shared_framework::godot {
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
    [[nodiscard]] void* module_handle() const noexcept { return module_; }

private:
    template <typename FunctionPointer>
    [[nodiscard]] bool load_symbol(FunctionPointer& out_function, const char* symbol_name);

private:
    void* module_ {nullptr};
    RuntimeBootstrapApi api_ {};
    std::string last_error_ {};
};

}  // namespace shared_framework::godot
