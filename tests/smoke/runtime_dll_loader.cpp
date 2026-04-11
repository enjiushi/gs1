#include "runtime_dll_loader.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdio>
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

RuntimeDllLoader::~RuntimeDllLoader() noexcept
{
    unload();
}

bool RuntimeDllLoader::load(const wchar_t* dll_path)
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

    return load_symbol(api_.get_api_version, "gs1_get_api_version") &&
        load_symbol(api_.get_build_string, "gs1_get_build_string") &&
        load_symbol(api_.create_runtime, "gs1_create_runtime") &&
        load_symbol(api_.destroy_runtime, "gs1_destroy_runtime") &&
        load_symbol(api_.submit_host_events, "gs1_submit_host_events") &&
        load_symbol(api_.submit_feedback_events, "gs1_submit_feedback_events") &&
        load_symbol(api_.run_phase1, "gs1_run_phase1") &&
        load_symbol(api_.run_phase2, "gs1_run_phase2") &&
        load_symbol(api_.pop_engine_command, "gs1_pop_engine_command");
}

void RuntimeDllLoader::unload() noexcept
{
    if (module_ != nullptr)
    {
        FreeLibrary(static_cast<HMODULE>(module_));
    }

    module_ = nullptr;
    api_ = {};
}

template <typename FunctionPointer>
bool RuntimeDllLoader::load_symbol(FunctionPointer& out_function, const char* symbol_name)
{
    auto* symbol = GetProcAddress(static_cast<HMODULE>(module_), symbol_name);
    if (symbol == nullptr)
    {
        last_error_ = std::string("GetProcAddress failed for ") + symbol_name + ": " +
            win32_error_message(GetLastError());
        unload();
        return false;
    }

    out_function = reinterpret_cast<FunctionPointer>(symbol);
    return true;
}
