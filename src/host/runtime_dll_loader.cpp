#include "gs1/host/runtime_dll_loader.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string>

namespace
{
std::string narrow_utf8_from_wide(const wchar_t* value)
{
    if (value == nullptr || *value == L'\0')
    {
        return {};
    }

    const int required_bytes = WideCharToMultiByte(
        CP_UTF8,
        0,
        value,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required_bytes <= 1)
    {
        return {};
    }

    std::string result(static_cast<std::size_t>(required_bytes), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value,
        -1,
        result.data(),
        required_bytes,
        nullptr,
        nullptr);
    result.pop_back();
    return result;
}

std::string win32_error_message(DWORD error_code)
{
    wchar_t* wide_message = nullptr;
    const DWORD written = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        0,
        reinterpret_cast<wchar_t*>(&wide_message),
        0,
        nullptr);
    if (written == 0 || wide_message == nullptr)
    {
        return "Win32 error " + std::to_string(static_cast<unsigned long long>(error_code));
    }

    const std::string message = narrow_utf8_from_wide(wide_message);
    LocalFree(wide_message);
    return message;
}
}  // namespace

namespace gs1::host {

RuntimeBootstrapDllLoader::~RuntimeBootstrapDllLoader() noexcept
{
    unload();
}

bool RuntimeBootstrapDllLoader::load(const wchar_t* dll_path)
{
    unload();

    HMODULE module = LoadLibraryW(dll_path);
    if (module == nullptr)
    {
        last_error_ = "LoadLibraryW failed for " + narrow_utf8_from_wide(dll_path) + ": " +
            win32_error_message(GetLastError());
        return false;
    }

    module_ = module;
    last_error_.clear();

    return load_symbol(api_.get_api_version, "gs_runtime_get_api_version") &&
        load_symbol(api_.get_build_string, "gs_runtime_get_build_string") &&
        load_symbol(api_.create_runtime, "gs_runtime_create") &&
        load_symbol(api_.destroy_runtime, "gs_runtime_destroy") &&
        load_symbol(api_.run_phase1, "gs_runtime_run_phase1") &&
        load_symbol(api_.run_phase2, "gs_runtime_run_phase2");
}

void RuntimeBootstrapDllLoader::unload() noexcept
{
    if (module_ != nullptr)
    {
        FreeLibrary(static_cast<HMODULE>(module_));
    }

    module_ = nullptr;
    api_ = {};
}

bool RuntimeBootstrapDllLoader::load_required_symbol(void** out_symbol, const char* symbol_name)
{
    if (out_symbol == nullptr)
    {
        last_error_ = "Output pointer for required symbol load was null.";
        unload();
        return false;
    }

    auto* symbol = GetProcAddress(static_cast<HMODULE>(module_), symbol_name);
    if (symbol == nullptr)
    {
        last_error_ = std::string("GetProcAddress failed for ") + symbol_name + ": " +
            win32_error_message(GetLastError());
        unload();
        return false;
    }

    *out_symbol = reinterpret_cast<void*>(symbol);
    return true;
}

template <typename FunctionPointer>
bool RuntimeBootstrapDllLoader::load_symbol(FunctionPointer& out_function, const char* symbol_name)
{
    void* raw_symbol = nullptr;
    if (!load_required_symbol(&raw_symbol, symbol_name))
    {
        return false;
    }

    out_function = reinterpret_cast<FunctionPointer>(raw_symbol);
    return true;
}

RuntimeDllLoader::~RuntimeDllLoader() noexcept
{
    unload();
}

bool RuntimeDllLoader::load(const wchar_t* dll_path)
{
    unload();

    if (!bootstrap_loader_.load(dll_path))
    {
        return false;
    }

    api_ = {};
    api_.bootstrap = bootstrap_loader_.api();
    return load_symbol(api_.submit_gameplay_action, "gs1_submit_gameplay_action") &&
        load_symbol(api_.submit_site_move_direction, "gs1_submit_site_move_direction") &&
        load_symbol(api_.submit_site_action_request, "gs1_submit_site_action_request") &&
        load_symbol(api_.submit_site_action_cancel, "gs1_submit_site_action_cancel") &&
        load_symbol(api_.submit_site_context_request, "gs1_submit_site_context_request") &&
        load_symbol(api_.submit_site_inventory_slot_tap, "gs1_submit_site_inventory_slot_tap") &&
        load_symbol(api_.submit_site_scene_ready, "gs1_submit_site_scene_ready") &&
        load_symbol(api_.get_game_state_view, "gs1_get_game_state_view") &&
        load_symbol(api_.query_site_tile_view, "gs1_query_site_tile_view") &&
        load_symbol(api_.get_runtime_profiling_snapshot, "gs1_get_runtime_profiling_snapshot") &&
        load_symbol(api_.reset_runtime_profiling, "gs1_reset_runtime_profiling") &&
        load_symbol(api_.set_runtime_profile_system_enabled, "gs1_set_runtime_profile_system_enabled");
}

void RuntimeDllLoader::unload() noexcept
{
    api_ = {};
    bootstrap_loader_.unload();
}

template <typename FunctionPointer>
bool RuntimeDllLoader::load_symbol(FunctionPointer& out_function, const char* symbol_name)
{
    void* raw_symbol = nullptr;
    if (!bootstrap_loader_.load_required_symbol(&raw_symbol, symbol_name))
    {
        return false;
    }

    out_function = reinterpret_cast<FunctionPointer>(raw_symbol);
    return true;
}

}  // namespace gs1::host

